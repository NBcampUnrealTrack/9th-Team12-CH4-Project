#include "Save/Backend/TDBackendSaveSubsystem.h"
#include "Save/Backend/TDBackendSaveCodec.h"
#include "Player/TDPlayerController.h"
#include "Player/TDPlayerState.h"
#include "Core/TDAccountSubSystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/GameInstance.h"

struct FTDBackendSession
{
    TWeakObjectPtr<ATDPlayerController> PC;
    FString Token, Lease;
    FGuid Character, RequestId;
    FGuid RecoveryId = FGuid::NewGuid();
    int64 Revision = 0;
    TArray<FTDCharacterSummary> Characters;
    FString Wanted, Sent, PendingBody;
    bool Busy = false, Saving = false, Leaving = false, Logout = false, Detached = false, Blocked = false,
         Renewing = false, WorldClosed = false, LeaseLost = false;
    double NextRenew = 0;
};
namespace
{
FString JsonText(const TSharedPtr<FJsonObject> &J)
{
    FString Text;
    if (J)
        FJsonSerializer::Serialize(J.ToSharedRef(),
                                   TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
    return Text;
}
TSharedPtr<FJsonObject> Parse(const FString &Text)
{
    TSharedPtr<FJsonObject> J;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J);
    return J;
}
FString Path(const FGuid &Id, const TCHAR *Tail)
{
    return TEXT("/v1/characters/") + Id.ToString(EGuidFormats::DigitsWithHyphens) + Tail;
}
bool ReadGuid(const TSharedPtr<FJsonObject> &J, const TCHAR *Key, FGuid &Out)
{
    FString V;
    return J && J->TryGetStringField(Key, V) && FGuid::Parse(V, Out) && Out.IsValid();
}
bool ReadRevision(const TSharedPtr<FJsonObject> &J, int64 &Out)
{
    double V;
    if (!J || !J->TryGetNumberField(TEXT("revision"), V) || !FMath::IsFinite(V) || V < 0 || V > 9007199254740991.0 ||
        V != FMath::FloorToDouble(V))
        return false;
    Out = static_cast<int64>(V);
    return true;
}
} // namespace
void UTDBackendSaveSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
    BaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("TD_BACKEND_URL"));
    ServerKey = FPlatformMisc::GetEnvironmentVariable(TEXT("TD_GAME_SERVER_KEY"));
#if WITH_EDITOR
    // Never mix a deployment URL with a local development secret.
    // This fallback is compiled out of packaged client/server targets.
    {
        TArray<FString> Lines;
        if (FFileHelper::LoadFileToStringArray(Lines, *FPaths::Combine(FPaths::ProjectDir(), TEXT("API/.env"))))
        {
            FString Port = TEXT("18080");
            FString LocalKey;
            FString SaveMode = TEXT("backend");
            for (FString Line : Lines)
            {
                Line.TrimStartAndEndInline();
                if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
                    continue;
                FString Name, Value;
                if (!Line.Split(TEXT("="), &Name, &Value))
                    continue;
                Name.TrimStartAndEndInline();
                Value.TrimStartAndEndInline();
                if (Value.Len() >= 2 &&
                    ((Value.StartsWith(TEXT("\"")) && Value.EndsWith(TEXT("\""))) ||
                     (Value.StartsWith(TEXT("'")) && Value.EndsWith(TEXT("'")))))
                    Value = Value.Mid(1, Value.Len() - 2);
                if (Name == TEXT("API_PORT"))
                    Port = Value.IsEmpty() ? TEXT("18080") : Value;
                else if (Name == TEXT("GAME_SERVER_API_KEY"))
                    LocalKey = Value;
                else if (Name == TEXT("TD_SAVE_MODE"))
                    SaveMode = Value.ToLower();
            }
            if (SaveMode == TEXT("dummy"))
            {
                BaseUrl.Empty();
                ServerKey.Empty();
                UE_LOG(LogTemp, Display, TEXT("[BackendSave] Editor dummy mode selected by API/.env."));
            }
            else if (SaveMode != TEXT("backend"))
            {
                BaseUrl = TEXT("http://127.0.0.1:0");
                ServerKey.Empty();
                UE_LOG(LogTemp, Error, TEXT("[BackendSave] Invalid TD_SAVE_MODE in API/.env: use dummy or backend. Storage requests blocked."));
            }
            else if (BaseUrl.IsEmpty() && ServerKey.IsEmpty())
            {
            ServerKey = MoveTemp(LocalKey);
            bool bDigitsOnly = !Port.IsEmpty() && Port.Len() <= 5;
            for (TCHAR C : Port)
                bDigitsOnly = bDigitsOnly && C >= TEXT('0') && C <= TEXT('9');
            const int32 PortNumber = bDigitsOnly ? FCString::Atoi(*Port) : 0;
            // Keep backend mode enabled on invalid config, so it cannot silently save to dummy memory.
            BaseUrl = TEXT("http://127.0.0.1:") + FString::FromInt(PortNumber);
            if (PortNumber < 1 || PortNumber > 65535 || ServerKey.Len() < 32)
            {
                ServerKey.Empty();
                UE_LOG(LogTemp, Error, TEXT("[BackendSave] Invalid API/.env: require API_PORT 1-65535 and GAME_SERVER_API_KEY at least 32 characters."));
            }
            else
                UE_LOG(LogTemp, Display, TEXT("[BackendSave] Editor local backend enabled at %s using API/.env."), *BaseUrl);
            }
        }
    }
#endif
    BaseUrl.RemoveFromEnd(TEXT("/"));
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick), 1.0f);
}
void UTDBackendSaveSubsystem::Deinitialize()
{
    bStopping = true;
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    Sessions.Empty();
    ServerKey.Empty();
    Super::Deinitialize();
}
TSharedPtr<FTDBackendSession> UTDBackendSaveSubsystem::Session(ATDPlayerController *PC)
{
    if (!IsEnabled() || !IsValid(PC) || !PC->HasAuthority() || PC->GetNetMode() == NM_Client)
        return nullptr;
    auto &S = Sessions.FindOrAdd(PC);
    if (!S)
    {
        S = MakeShared<FTDBackendSession>();
        S->PC = PC;
    }
    return S;
}
void UTDBackendSaveSubsystem::Report(const TSharedPtr<FTDBackendSession> &S, const FString &Message)
{
    if (auto *PC = S->PC.Get())
        PC->ClientAccountMessage(Message);
    else
        UE_LOG(LogTemp, Warning, TEXT("[BackendSave] %s"), *Message);
}
void UTDBackendSaveSubsystem::Request(const TSharedPtr<FTDBackendSession> &S, const FString &Method,
                                      const FString &UrlPath, const FString &Body, FReply Reply, int32 Retries)
{
    if (bStopping)
        return;
    if (ServerKey.Len() < 32 ||
        !(BaseUrl.StartsWith(TEXT("https://")) || BaseUrl.StartsWith(TEXT("http://127.0.0.1:")) ||
          BaseUrl.StartsWith(TEXT("http://localhost:"))))
    {
        Reply(0, nullptr);
        return;
    }
    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(BaseUrl + UrlPath);
    Req->SetVerb(Method);
    Req->SetTimeout(10.0f);
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetHeader(TEXT("X-Game-Server-Key"), ServerKey);
    if (!S->Token.IsEmpty())
        Req->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + S->Token);
    if (!S->Lease.IsEmpty())
        Req->SetHeader(TEXT("X-Character-Lease"), S->Lease);
    if (!Body.IsEmpty())
        Req->SetContentAsString(Body);
    TWeakObjectPtr<UTDBackendSaveSubsystem> Weak(this);
    auto Done = MakeShared<FReply>(MoveTemp(Reply));
    Req->OnProcessRequestComplete().BindLambda(
        [Weak, S, Method, UrlPath, Body, Done, Retries](FHttpRequestPtr, FHttpResponsePtr Resp, bool Ok)
        {
            auto *Self = Weak.Get();
            if (!Self || Self->bStopping)
                return;
            const int32 Code = Ok && Resp ? Resp->GetResponseCode() : 0;
            if ((Code == 0 || Code >= 500) && Retries > 0)
            {
                Self->Request(S, Method, UrlPath, Body, *Done, Retries - 1);
                return;
            }
            (*Done)(Code,
                    Resp && Resp->GetContentLength() <= 1024 * 1024 ? Parse(Resp->GetContentAsString()) : nullptr);
        });
    if (!Req->ProcessRequest())
    {
        Req->OnProcessRequestComplete().Unbind();
        (*Done)(0, nullptr);
    }
}
void UTDBackendSaveSubsystem::Login(ATDPlayerController *PC, const FString &Id, const FString &Password, bool bRegister)
{
    auto S = Session(PC);
    if (!S || S->Busy || !PC->GetPlayerState<ATDPlayerState>() || PC->IsLoggedIn() ||
        PC->GetPlayerState<ATDPlayerState>()->HasSelectedCharacter())
        return;
    S->Busy = true;
    auto J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("login_id"), Id);
    J->SetStringField(TEXT("password"), Password);
    Request(S, TEXT("POST"), bRegister ? TEXT("/v1/auth/register") : TEXT("/v1/auth/login"), JsonText(J),
            [this, S, bRegister](int32 Code, const TSharedPtr<FJsonObject> &R)
            {
                S->Busy = false;
                auto *P = S->PC.Get();
                if (!P || S->Detached)
                {
                    FString Token;
                    if (!bRegister && Code == 200 && R && R->TryGetStringField(TEXT("access_token"), Token))
                        S->Token = Token;
                    FinishLeave(S);
                    return;
                }
                if (bRegister)
                {
                    P->ClientAccountActionResult(TEXT("Register"), Code == 201,
                                                 Code == 201
                                                     ? TEXT("가입했습니다. 로그인해 주세요.")
                                                     : TEXT("가입에 실패했습니다. 입력 또는 연결 상태를 확인하세요."));
                    return;
                }
                FGuid Id;
                FString Token;
                if (Code != 200 || !ReadGuid(R, TEXT("account_id"), Id) ||
                    !R->TryGetStringField(TEXT("access_token"), Token) || Token.Len() != 64)
                {
                    Report(S, TEXT("로그인에 실패했습니다."));
                    return;
                }
                S->Token = Token;
                P->DummyAccountId = Id;
                P->ForceNetUpdate();
                Refresh(S);
            });
}
void UTDBackendSaveSubsystem::Refresh(const TSharedPtr<FTDBackendSession> &S, FName Action)
{
    S->Busy = true;
    Request(S, TEXT("GET"), TEXT("/v1/characters"), {},
            [this, S, Action](int32 Code, const TSharedPtr<FJsonObject> &R)
            {
                S->Busy = false;
                auto *P = S->PC.Get();
                if (!P || S->Detached)
                    return;
                const TArray<TSharedPtr<FJsonValue>> *Rows = nullptr;
                TArray<FTDCharacterSummary> List;
                TSet<FGuid> Ids;
                TSet<int32> Slots;
                bool Valid = Code == 200 && R && R->TryGetArrayField(TEXT("characters"), Rows) && Rows->Num() <= 6;
                if (Valid)
                    for (const auto &Row : *Rows)
                    {
                        if (Row->Type != EJson::Object)
                        {
                            Valid = false;
                            break;
                        }
                        auto O = Row->AsObject();
                        FTDCharacterSummary Item;
                        FString Name, Class;
                        double Level, Slot;
                        if (!ReadGuid(O, TEXT("character_id"), Item.CharacterId) || Ids.Contains(Item.CharacterId) ||
                            !O->TryGetStringField(TEXT("character_name"), Name) ||
                            !O->TryGetStringField(TEXT("class_id"), Class) ||
                            !O->TryGetNumberField(TEXT("level"), Level) || Level < 1 || Level > MAX_int32 ||
                            !O->TryGetNumberField(TEXT("slot_index"), Slot) || Slot < 0 || Slot > 5 ||
                            Slot != FMath::FloorToDouble(Slot) || Slots.Contains(static_cast<int32>(Slot)))
                        {
                            Valid = false;
                            break;
                        }
                        Ids.Add(Item.CharacterId);
                        Slots.Add(static_cast<int32>(Slot));
                        Item.CharacterName = Name;
                        Item.ClassId = FName(*Class);
                        Item.Level = static_cast<int32>(Level);
                        const TArray<TSharedPtr<FJsonValue>> *Gear = nullptr;
                        if (!O->TryGetArrayField(TEXT("equipped_item_ids"), Gear))
                        {
                            Valid = false;
                            break;
                        }
                        for (const auto &Value : *Gear)
                        {
                            FString ItemId;
                            if (!Value->TryGetString(ItemId))
                            {
                                Valid = false;
                                break;
                            }
                            Item.EquippedItemIds.Add(FName(*ItemId));
                        }
                        List.Add(Item);
                    }
                if (!Valid)
                {
                    Report(S, TEXT("캐릭터 목록을 불러오지 못했습니다. 다시 로그인해 주세요."));
                    return;
                }
                S->Characters = List;
                P->GetPlayerState<ATDPlayerState>()->SetCharacterSlots(List);
                P->ForceNetUpdate();
                if (!Action.IsNone())
                    P->ClientAccountActionResult(Action, true, TEXT("처리했습니다."));
                else
                    Report(S, TEXT("캐릭터를 선택하세요."));
            });
}
void UTDBackendSaveSubsystem::Select(ATDPlayerController *PC, int32 ListIndex)
{
    auto S = Session(PC);
    if (!S || S->Busy || S->Character.IsValid() || S->Token.IsEmpty() || !S->Characters.IsValidIndex(ListIndex) ||
        PC->GetPlayerState<ATDPlayerState>()->HasSelectedCharacter())
        return;
    S->Busy = true;
    S->Character = S->Characters[ListIndex].CharacterId;
    S->Lease = FGuid::NewGuid().ToString(EGuidFormats::Digits) + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FGuid Id = S->Character;
    Request(
        S, TEXT("POST"), Path(Id, TEXT("/lease")), {},
        [this, S, Id, ListIndex](int32 Code, const TSharedPtr<FJsonObject> &)
        {
            if (Code != 204)
            {
                S->Busy = false;
                S->Character.Invalidate();
                S->Lease.Empty();
                Report(S, TEXT("캐릭터가 사용 중이거나 연결할 수 없습니다."));
                return;
            }
            S->NextRenew = FPlatformTime::Seconds() + 30;
            if (S->Detached)
            {
                FinishLeave(S);
                return;
            }
            Request(
                S, TEXT("GET"), Path(Id, TEXT("/save")), {},
                [this, S, Id, ListIndex](int32 Status, const TSharedPtr<FJsonObject> &R)
                {
                    auto *P = S->PC.Get();
                    if (S->Detached || !P)
                    {
                        FinishLeave(S);
                        return;
                    }
                    const TSharedPtr<FJsonObject> *Data = nullptr;
                    FTDDummyCharacterRecord Record;
                    FGuid Returned;
                    int64 Revision;
                    if (Status != 200 || !ReadGuid(R, TEXT("character_id"), Returned) || Returned != Id ||
                        !ReadRevision(R, Revision) || !R->TryGetObjectField(TEXT("data"), Data) ||
                        !TDBackendSaveCodec::Decode(*Data, Record.Data))
                    {
                        Report(S, TEXT("저장 데이터를 복원할 수 없습니다."));
                        FinishLeave(S);
                        return;
                    }
                    S->Revision = Revision;
                    S->RecoveryId = FGuid::NewGuid();
                    S->Blocked = false;
                    S->WorldClosed = false;
                    S->LeaseLost = false;
                    S->Logout = false;
                    Record.CharacterId = Id;
                    Record.CharacterName = S->Characters[ListIndex].CharacterName;
                    P->ApplyAccountRecord(Record, ListIndex);
                    S->Busy = false;
                    if (!P->GetPawn())
                    {
                        Report(S, TEXT("스폰에 실패했습니다."));
                        S->Leaving = true;
                        FinishLeave(S);
                    }
                },
                2);
        },
        2);
}
void UTDBackendSaveSubsystem::Create(ATDPlayerController *PC, const FString &Name, FName ClassId)
{
    auto S = Session(PC);
    if (!S || S->Busy || S->Character.IsValid() || S->Token.IsEmpty())
        return;
    S->Busy = true;
    auto J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("character_name"), Name);
    J->SetStringField(TEXT("class_id"), ClassId.ToString());
    Request(S, TEXT("POST"), TEXT("/v1/characters"), JsonText(J),
            [this, S](int32 Code, const TSharedPtr<FJsonObject> &)
            {
                S->Busy = false;
                if (S->Detached)
                    return;
                if (Code == 201)
                    Refresh(S, TEXT("Create"));
                else
                {
                    if (auto *P = S->PC.Get())
                        P->ClientAccountActionResult(TEXT("Create"), false,
                                                     TEXT("생성 결과를 확인하지 못했습니다. 목록을 갱신합니다."));
                    Refresh(S);
                }
            });
}
void UTDBackendSaveSubsystem::Delete(ATDPlayerController *PC, const FGuid &Id)
{
    auto S = Session(PC);
    if (!S || S->Busy || S->Character.IsValid() || S->Token.IsEmpty() ||
        !S->Characters.ContainsByPredicate([Id](const auto &C) { return C.CharacterId == Id; }))
        return;
    S->Busy = true;
    Request(S, TEXT("DELETE"), Path(Id, TEXT("")), {},
            [this, S](int32 Code, const TSharedPtr<FJsonObject> &)
            {
                S->Busy = false;
                if (S->Detached)
                    return;
                if (Code == 204)
                    Refresh(S, TEXT("Delete"));
                else
                {
                    if (auto *P = S->PC.Get())
                        P->ClientAccountActionResult(TEXT("Delete"), false,
                                                     TEXT("삭제할 수 없습니다. 사용 중 여부를 확인하세요."));
                    Refresh(S);
                }
            });
}
bool UTDBackendSaveSubsystem::Capture(const TSharedPtr<FTDBackendSession> &S, FString &Out)
{
    auto *PC = S->PC.Get();
    FTDPlayerSaveData Data;
    if (!PC || !PC->CaptureAccountData(Data))
        return false;
    Out = JsonText(TDBackendSaveCodec::Encode(Data));
    return !Out.IsEmpty();
}
bool UTDBackendSaveSubsystem::Save(ATDPlayerController *PC)
{
    auto S = Session(PC);
    if (!S || S->Busy || !S->Character.IsValid() || S->Blocked || !Capture(S, S->Wanted))
        return false;
    if (S->Saving)
    {
        if (!WriteRecovery(S))
            return false;
    }
    else
        StartSave(S);
    return true;
}
bool UTDBackendSaveSubsystem::WriteRecovery(const TSharedPtr<FTDBackendSession> &S)
{
    if (!S->Character.IsValid() || S->Wanted.IsEmpty())
        return false;
    const FString Dir = FPaths::ProjectSavedDir() / TEXT("BackendOutbox");
    if (!IFileManager::Get().MakeDirectory(*Dir, true))
        return false;
    auto J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("character_id"), S->Character.ToString(EGuidFormats::DigitsWithHyphens));
    J->SetObjectField(TEXT("latest_data"), Parse(S->Wanted));
    if (!S->PendingBody.IsEmpty())
        J->SetObjectField(TEXT("pending_request"), Parse(S->PendingBody));
    // No bearer token, lease token, or server key is written to the recovery file.
    const FString File = Dir / (S->Character.ToString(EGuidFormats::Digits) + TEXT("-") +
                                S->RecoveryId.ToString(EGuidFormats::Digits) + TEXT(".json"));
    if (!FFileHelper::SaveStringToFile(JsonText(J), *(File + TEXT(".tmp")),
                                       FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        return false;
    return IFileManager::Get().Move(*File, *(File + TEXT(".tmp")), true, true);
}
void UTDBackendSaveSubsystem::Quarantine(const TSharedPtr<FTDBackendSession> &S)
{
    S->Blocked = true;
    if (!S->Detached)
        Capture(S, S->Wanted);
    if (!S->Wanted.IsEmpty() && !WriteRecovery(S))
        Report(S, TEXT("복구 파일을 기록하지 못했습니다. 서버 디스크를 확인하세요."));
    S->Logout = true;
    S->Leaving = true;
    FinishLeave(S);
    Report(S, TEXT("저장 충돌 또는 사용권 만료로 플레이를 종료했습니다. 복구 데이터는 서버 Saved/BackendOutbox에 "
                   "보관됩니다."));
}
void UTDBackendSaveSubsystem::StartSave(const TSharedPtr<FTDBackendSession> &S)
{
    if (S->PendingBody.IsEmpty())
    {
        S->Sent = S->Wanted;
        S->RequestId = FGuid::NewGuid();
        auto J = MakeShared<FJsonObject>();
        J->SetStringField(TEXT("request_id"), S->RequestId.ToString(EGuidFormats::DigitsWithHyphens));
        J->SetNumberField(TEXT("expected_revision"), S->Revision);
        J->SetObjectField(TEXT("data"), Parse(S->Sent));
        S->PendingBody = JsonText(J);
    }
    if (!WriteRecovery(S))
    {
        Report(S, TEXT("저장 복구 파일을 기록할 수 없습니다. 서버 디스크를 확인하세요."));
        return;
    }
    S->Saving = true;
    const FGuid Id = S->Character, RequestId = S->RequestId;
    Request(
        S, TEXT("PUT"), Path(Id, TEXT("/save")), S->PendingBody,
        [this, S, Id, RequestId](int32 Code, const TSharedPtr<FJsonObject> &R)
        {
            S->Saving = false;
            int64 Revision;
            FGuid Returned, ReturnedRequest;
            if (Code != 200 || !ReadRevision(R, Revision) || Revision != S->Revision + 1 ||
                !ReadGuid(R, TEXT("character_id"), Returned) || Returned != Id ||
                !ReadGuid(R, TEXT("request_id"), ReturnedRequest) || ReturnedRequest != RequestId)
            {
                if (Code >= 400 && Code < 500 && Code != 429)
                {
                    Quarantine(S);
                    return;
                }
                Report(S, TEXT("저장 완료를 확인하지 못했습니다. 상태를 보존했습니다. TDSave로 재시도하세요. 충돌 시 "
                               "서버 확인이 필요합니다."));
                return;
            }
            S->Revision = Revision;
            S->PendingBody.Empty();
            if (S->LeaseLost)
            {
                Quarantine(S);
                return;
            }
            if (!S->Detached && !Capture(S, S->Wanted))
            {
                S->Blocked = true;
                Report(S, TEXT("최신 상태를 수집할 수 없습니다."));
                return;
            }
            if (S->Wanted != S->Sent)
            {
                StartSave(S);
                return;
            }
            IFileManager::Get().Delete(*(FPaths::ProjectSavedDir() / TEXT("BackendOutbox") /
                                         (Id.ToString(EGuidFormats::Digits) + TEXT("-") +
                                          S->RecoveryId.ToString(EGuidFormats::Digits) + TEXT(".json"))),
                                       false, true);
            if (auto *P = S->PC.Get(); P && !S->Detached)
            {
                FTDPlayerSaveData Saved;
                if (TDBackendSaveCodec::Decode(Parse(S->Sent), Saved))
                    for (auto &C : S->Characters)
                        if (C.CharacterId == Id)
                        {
                            C.Level = Saved.Level;
                            C.EquippedItemIds.Empty();
                            for (const auto &Item : Saved.EquippedItems)
                                C.EquippedItemIds.Add(Item.ItemId);
                        }
                P->GetPlayerState<ATDPlayerState>()->SetCharacterSlots(S->Characters);
            }
            Report(S, TEXT("백엔드에 저장했습니다."));
            if (S->Leaving || S->Detached)
                FinishLeave(S);
        },
        2);
}
void UTDBackendSaveSubsystem::Leave(ATDPlayerController *PC, bool bLogout)
{
    auto S = Session(PC);
    if (!S || S->Busy || S->Blocked)
        return;
    S->Leaving = true;
    S->Logout = bLogout;
    if (S->WorldClosed)
        FinishLeave(S);
    else if (S->Character.IsValid())
    {
        if (!Save(PC))
            Report(S, TEXT("최종 저장을 시작하지 못했습니다."));
    }
    else
        FinishLeave(S);
}
void UTDBackendSaveSubsystem::FinishLeave(const TSharedPtr<FTDBackendSession> &S)
{
    S->Busy = true;
    // Close gameplay immediately after the acknowledged snapshot, before the release HTTP wait.
    if (!S->WorldClosed)
    {
        if (auto *P = S->PC.Get(); P && !S->Detached)
            P->FinishBackendLeave(S->Characters, S->Logout);
        S->WorldClosed = true;
    }
    auto Finish = [this, S](int32 Code, const TSharedPtr<FJsonObject> &)
    {
        if (Code != 204)
        {
            Report(S, TEXT("캐릭터 사용권 해제를 확인하지 못했습니다. 다시 시도하세요."));
            S->Busy = false;
            return;
        }
        S->Character.Invalidate();
        S->Lease.Empty();
        S->PendingBody.Empty();
        S->Wanted.Empty();
        S->Sent.Empty();
        S->Busy = false;
        S->Leaving = false;
        if (S->Logout && !S->Token.IsEmpty())
        {
            S->Busy = true;
            Request(
                S, TEXT("POST"), TEXT("/v1/auth/logout"), {},
                [this, S](int32, const TSharedPtr<FJsonObject> &)
                {
                    S->Token.Empty();
                    S->Busy = false;
                    if (S->Detached)
                        Sessions.Remove(S->PC);
                },
                2);
        }
        else if (S->Detached)
            Sessions.Remove(S->PC);
        S->WorldClosed = false;
    };
    if (S->Character.IsValid())
        Request(S, TEXT("DELETE"), Path(S->Character, TEXT("/lease")), {}, MoveTemp(Finish), 2);
    else
        Finish(204, nullptr);
}
void UTDBackendSaveSubsystem::Disconnect(ATDPlayerController *PC)
{
    auto *Found = Sessions.Find(PC);
    if (!Found)
        return;
    auto S = *Found;
    if (S->Detached)
        return;
    if (S->Character.IsValid() && !S->Busy && !S->Blocked)
    {
        Capture(S, S->Wanted);
        if (!S->Wanted.IsEmpty())
            WriteRecovery(S);
    }
    S->Detached = true;
    S->Logout = true;
    if (S->Character.IsValid() && !S->Busy && !S->Saving && !S->Blocked && !S->Wanted.IsEmpty())
        StartSave(S);
    else if (!S->Character.IsValid() && !S->Busy)
        FinishLeave(S);
}
bool UTDBackendSaveSubsystem::Tick(float)
{
    const double Now = FPlatformTime::Seconds();
    for (auto &Pair : Sessions)
    {
        auto S = Pair.Value;
        if (!S->Character.IsValid() || S->Busy || S->Renewing || Now < S->NextRenew)
            continue;
        if (S->WorldClosed)
        {
            S->NextRenew = Now + 30;
            FinishLeave(S);
            continue;
        }
        if (S->Blocked)
            continue;
        S->Renewing = true;
        S->NextRenew = Now + 30;
        const auto Id = S->Character;
        Request(
            S, TEXT("PUT"), Path(Id, TEXT("/lease")), {},
            [this, S, Id](int32 Code, const TSharedPtr<FJsonObject> &)
            {
                S->Renewing = false;
                if (Id != S->Character || S->WorldClosed)
                    return;
                if (Code == 409)
                {
                    if (S->Saving)
                    {
                        S->LeaseLost = true;
                        return;
                    }
                    Quarantine(S);
                }
                else if (Code == 204 && !S->Saving && !S->PendingBody.IsEmpty())
                    StartSave(S);
            },
            2);
    }
    return true;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "UObject/StrongObjectPtr.h"

class FTDBackendWait final : public IAutomationLatentCommand
{
  public:
    explicit FTDBackendWait(TFunction<bool()> InPoll) : Poll(MoveTemp(InPoll))
    {
    }
    virtual bool Update() override
    {
        return Poll();
    }

  private:
    TFunction<bool()> Poll;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTDBackendHttpTest, "TD.Save.Backend.HttpQueue",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTDBackendHttpTest::RunTest(const FString &)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("TDBackendIntegration")))
    {
        AddInfo(TEXT("Isolated integration endpoint not requested."));
        return true;
    }
    struct FRun
    {
        TStrongObjectPtr<UGameInstance> GameInstance;
        TStrongObjectPtr<UTDBackendSaveSubsystem> Service;
        TSharedPtr<FTDBackendSession> Session = MakeShared<FTDBackendSession>();
        FGuid Id;
        bool Started = false, Checking = false, Done = false, Conflict = false;
        double Deadline = FPlatformTime::Seconds() + 90;
    };
    auto Run = MakeShared<FRun>();
    Run->GameInstance.Reset(NewObject<UGameInstance>());
    Run->Service.Reset(NewObject<UTDBackendSaveSubsystem>(Run->GameInstance.Get()));
    auto *B = Run->Service.Get();
    B->BaseUrl = TEXT("http://127.0.0.1:18081");
    B->ServerKey = TEXT("td-integration-server-key-0000000000000000000000000000000000000000");
    auto S = Run->Session;
    S->Detached = true;
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(10);
    auto Credentials = MakeShared<FJsonObject>();
    Credentials->SetStringField(TEXT("login_id"), TEXT("ue_") + Suffix);
    Credentials->SetStringField(TEXT("password"), TEXT("TestPassword123!"));
    const FString Body = JsonText(Credentials);
    B->Request(S, TEXT("POST"), TEXT("/v1/auth/register"), Body,
               [this, Run, Body, Suffix](int32 Code, const TSharedPtr<FJsonObject> &)
               {
                   if (!TestEqual(TEXT("HTTP register"), Code, 201))
                   {
                       Run->Done = true;
                       return;
                   }
                   auto *Service = Run->Service.Get();
                   auto Session = Run->Session;
                   Service->Request(
                       Session, TEXT("POST"), TEXT("/v1/auth/login"), Body,
                       [this, Run, Suffix](int32 Status, const TSharedPtr<FJsonObject> &R)
                       {
                           if (!TestTrue(TEXT("HTTP login"),
                                         Status == 200 && R &&
                                             R->TryGetStringField(TEXT("access_token"), Run->Session->Token)))
                           {
                               Run->Done = true;
                               return;
                           }
                           auto C = MakeShared<FJsonObject>();
                           C->SetStringField(TEXT("character_name"), TEXT("UE") + Suffix);
                           C->SetStringField(TEXT("class_id"), TEXT("Warrior"));
                           Run->Service->Request(
                               Run->Session, TEXT("POST"), TEXT("/v1/characters"), JsonText(C),
                               [this, Run](int32 Created, const TSharedPtr<FJsonObject> &C)
                               {
                                   if (!TestTrue(TEXT("HTTP create"),
                                                 Created == 201 && ReadGuid(C, TEXT("character_id"), Run->Id)))
                                   {
                                       Run->Done = true;
                                       return;
                                   }
                                   Run->Session->Character = Run->Id;
                                   Run->Session->Lease = FGuid::NewGuid().ToString(EGuidFormats::Digits) +
                                                         FGuid::NewGuid().ToString(EGuidFormats::Digits);
                                   Run->Service->Request(Run->Session, TEXT("POST"), Path(Run->Id, TEXT("/lease")), {},
                                                         [this, Run](int32 Claimed, const TSharedPtr<FJsonObject> &)
                                                         {
                                                             if (!TestEqual(TEXT("HTTP lease"), Claimed, 204))
                                                             {
                                                                 Run->Done = true;
                                                                 return;
                                                             }
                                                             FString Fixture;
                                                             FFileHelper::LoadFileToString(
                                                                 Fixture, *(FPaths::ProjectSavedDir() /
                                                                            TEXT("Automation/backend-codec.json")));
                                                             auto Data = Parse(Fixture);
                                                             if (!TestTrue(TEXT("Wire fixture exists"), Data.IsValid()))
                                                             {
                                                                 Run->Done = true;
                                                                 return;
                                                             }
                                                             Data->SetNumberField(TEXT("Gold"), 1);
                                                             Run->Session->Wanted = JsonText(Data);
                                                             Run->Service->StartSave(Run->Session);
                                                             // A new snapshot arrives while revision 0 is in flight.
                                                             Data->SetNumberField(TEXT("Gold"), 12345);
                                                             Run->Session->Wanted = JsonText(Data);
                                                             Run->Started = true;
                                                         });
                               });
                       });
               });
    ADD_LATENT_AUTOMATION_COMMAND(FTDBackendWait(
        [this, Run]()
        {
            if (Run->Done)
                return true;
            if (FPlatformTime::Seconds() > Run->Deadline)
            {
                AddError(TEXT("HTTP queue test timed out"));
                return true;
            }
            if (Run->Conflict && !Run->Session->Character.IsValid() && !Run->Session->Busy)
            {
                TestTrue(TEXT("409 blocks stale save"), Run->Session->Blocked);
                const FString File = FPaths::ProjectSavedDir() / TEXT("BackendOutbox") /
                                     (Run->Id.ToString(EGuidFormats::Digits) + TEXT("-") +
                                      Run->Session->RecoveryId.ToString(EGuidFormats::Digits) + TEXT(".json"));
                FString Recovery;
                TestTrue(TEXT("Conflict snapshot retained"), FFileHelper::LoadFileToString(Recovery, *File));
                TestFalse(TEXT("No bearer in recovery file"), Recovery.Contains(TEXT("access_token")));
                auto Record = Parse(Recovery);
                if (TestTrue(TEXT("Recovery is readable"), Record.IsValid()))
                    TestEqual(
                        TEXT("Original revision preserved"),
                        Record->GetObjectField(TEXT("pending_request"))->GetNumberField(TEXT("expected_revision")),
                        0.0);
                IFileManager::Get().Delete(*File, false, true);
                Run->Done = true;
                return true;
            }
            if (Run->Started && !Run->Session->Character.IsValid() && !Run->Session->Busy && !Run->Checking)
            {
                Run->Checking = true;
                TestEqual(TEXT("Two snapshots serialized"), Run->Session->Revision, int64(2));
                Run->Service->Request(
                    Run->Session, TEXT("GET"), Path(Run->Id, TEXT("/save")), {},
                    [this, Run](int32 Code, const TSharedPtr<FJsonObject> &J)
                    {
                        const TSharedPtr<FJsonObject> *Data = nullptr;
                        if (TestTrue(TEXT("Read final DB state"),
                                     Code == 200 && J && J->TryGetObjectField(TEXT("data"), Data)))
                        {
                            TestEqual(TEXT("No newer snapshot lost"), (*Data)->GetNumberField(TEXT("Gold")), 12345.0);
                            TestEqual(TEXT("DB revision"), J->GetNumberField(TEXT("revision")), 2.0);
                        }
                        if (!Data)
                        {
                            Run->Done = true;
                            return;
                        }
                        Run->Session->Character = Run->Id;
                        Run->Session->Lease = FGuid::NewGuid().ToString(EGuidFormats::Digits) +
                                              FGuid::NewGuid().ToString(EGuidFormats::Digits);
                        Run->Session->Wanted = JsonText(*Data);
                        Run->Session->Revision = 0;
                        Run->Service->Request(Run->Session, TEXT("POST"), Path(Run->Id, TEXT("/lease")), {},
                                              [this, Run](int32 Claimed, const TSharedPtr<FJsonObject> &)
                                              {
                                                  if (!TestEqual(TEXT("Previous lease was released"), Claimed, 204))
                                                  {
                                                      Run->Done = true;
                                                      return;
                                                  }
                                                  Run->Conflict = true;
                                                  Run->Service->StartSave(Run->Session);
                                              });
                    });
            }
            return Run->Done;
        }));
    return true;
}
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/TDGameMode.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerStart.h"
#include "Items/TDInventoryComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTDBackendControllerTest, "TD.Save.Backend.ControllerFlow",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTDBackendControllerTest::RunTest(const FString &)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("TDBackendIntegration")))
    {
        AddInfo(TEXT("Isolated integration endpoint not requested."));
        return true;
    }
    struct FRun
    {
        TStrongObjectPtr<UGameInstance> GI;
        UWorld *World = nullptr;
        UTDBackendSaveSubsystem *Service = nullptr;
        ATDPlayerController *PC = nullptr;
        TSharedPtr<FTDBackendSession> Session;
        FString Login, Name;
        FGuid Selected;
        int32 Phase = 0;
        double Deadline = FPlatformTime::Seconds() + 120;
        void Cleanup()
        {
            GI->Shutdown();
            GEngine->DestroyWorldContext(World);
            World->DestroyWorld(false);
        }
    };
    auto R = MakeShared<FRun>();
    R->GI.Reset(NewObject<UGameInstance>(GEngine));
    R->GI->InitializeStandalone();
    R->World = R->GI->GetWorld();
    R->World->GetWorldSettings()->DefaultGameMode = ATDGameMode::StaticClass();
    if (!TestTrue(TEXT("Create game mode"), R->World->SetGameMode(FURL())))
    {
        R->Cleanup();
        return false;
    }
    R->World->InitializeActorsForPlay(FURL());
    auto *Start = R->World->SpawnActor<APlayerStart>();
    Start->PlayerStartTag = TEXT("Zone.Region1.Town");
    R->Service = R->GI->GetSubsystem<UTDBackendSaveSubsystem>();
    R->Service->BaseUrl = TEXT("http://127.0.0.1:18081");
    R->Service->ServerKey = TEXT("td-integration-server-key-0000000000000000000000000000000000000000");
    R->PC = R->World->SpawnActor<ATDPlayerController>();
    R->PC->bStartAccountFlowOnBeginPlay = false;
    if (!TestNotNull(TEXT("Controller PlayerState"), R->PC->GetPlayerState<ATDPlayerState>()))
    {
        R->Cleanup();
        return false;
    }
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(9);
    R->Login = TEXT("flow_") + Suffix;
    R->Name = TEXT("흐름") + Suffix;
    R->PC->ServerRegisterAccount_Implementation(R->Login, TEXT("TestPassword123!"));
    R->Session = R->Service->Session(R->PC);
    ADD_LATENT_AUTOMATION_COMMAND(FTDBackendWait(
        [this, R]()
        {
            if (FPlatformTime::Seconds() > R->Deadline)
            {
                AddError(FString::Printf(TEXT("Controller flow timeout at phase %d"), R->Phase));
                R->Cleanup();
                return true;
            }
            if (R->Session->Busy || R->Session->Saving)
                return false;
            auto *PS = IsValid(R->PC) ? R->PC->GetPlayerState<ATDPlayerState>() : nullptr;
            switch (R->Phase++)
            {
            case 0:
                R->PC->ServerLogin_Implementation(R->Login, TEXT("TestPassword123!"));
                break;
            case 1:
                if (!TestTrue(TEXT("Controller logged in"), R->PC->IsLoggedIn()))
                {
                    R->Cleanup();
                    return true;
                }
                R->PC->ServerCreateCharacter_Implementation(R->Name + TEXT("0"), TEXT("Warrior"));
                break;
            case 2:
                R->PC->ServerCreateCharacter_Implementation(R->Name + TEXT("1"), TEXT("Warrior"));
                break;
            case 3:
                R->PC->ServerCreateCharacter_Implementation(R->Name + TEXT("2"), TEXT("Warrior"));
                break;
            case 4:
                if (!TestEqual(TEXT("Three controller characters"), R->Session->Characters.Num(), 3))
                {
                    R->Cleanup();
                    return true;
                }
                R->Selected = R->Session->Characters[2].CharacterId;
                R->PC->ServerDeleteCharacter_Implementation(R->Session->Characters[1].CharacterId);
                break;
            case 5:
                if (!TestEqual(TEXT("Deleted middle list row"), R->Session->Characters.Num(), 2))
                {
                    R->Cleanup();
                    return true;
                }
                R->PC->ServerSelectCharacter_Implementation(1);
                break;
            case 6:
                if (!TestTrue(TEXT("Selected by UUID despite slot gap"), R->PC->DummyCharacterId == R->Selected) ||
                    !TestNotNull(TEXT("Pawn spawned after restore"), R->PC->GetPawn().Get()))
                {
                    R->Cleanup();
                    return true;
                }
                PS->GetInventoryComponent()->AddGold(100);
                R->PC->ServerSaveCharacter_Implementation();
                PS->GetInventoryComponent()->AddGold(25);
                break;
            case 7:
                TestTrue(TEXT("Queued latest snapshot acknowledged"), R->Session->Revision >= 2);
                R->PC->ServerLeaveCharacter_Implementation(false);
                break;
            case 8:
                TestTrue(TEXT("Return keeps login"), R->PC->IsLoggedIn());
                TestFalse(TEXT("Old PlayerState replaced"), PS->HasSelectedCharacter());
                TestNull(TEXT("Old Pawn removed"), R->PC->GetPawn().Get());
                R->PC->ServerSelectCharacter_Implementation(1);
                break;
            case 9:
                if (!TestNotNull(TEXT("Restored Pawn"), R->PC->GetPawn().Get()))
                {
                    R->Cleanup();
                    return true;
                }
                TestEqual(TEXT("Gold survives character return"), PS->GetInventoryComponent()->GetGold(), 125);
                R->PC->GetPawn()->SetActorLocation(FVector(321, 654, 987));
                PS->GetInventoryComponent()->AddGold(50);
                R->PC->Destroy();
                break;
            case 10:
                if (R->Session->Character.IsValid())
                {
                    --R->Phase;
                    return false;
                }
                R->PC = R->World->SpawnActor<ATDPlayerController>();
                R->PC->bStartAccountFlowOnBeginPlay = false;
                R->PC->ServerLogin_Implementation(R->Login, TEXT("TestPassword123!"));
                R->Session = R->Service->Session(R->PC);
                break;
            case 11:
                R->PC->ServerSelectCharacter_Implementation(1);
                break;
            case 12:
                if (!TestNotNull(TEXT("Reconnect Pawn"), R->PC->GetPawn().Get()))
                {
                    R->Cleanup();
                    return true;
                }
                TestEqual(TEXT("Disconnect gold persisted"), PS->GetInventoryComponent()->GetGold(), 175);
                TestEqual(TEXT("Disconnect captured Pawn before destruction"), R->PC->GetPawn()->GetActorLocation(),
                          FVector(321, 654, 987));
                R->PC->ServerLeaveCharacter_Implementation(true);
                break;
            default:
                TestFalse(TEXT("Final logout clears identity"), R->PC->IsLoggedIn());
                TestTrue(TEXT("Final logout clears token"), R->Session->Token.IsEmpty());
                R->Cleanup();
                return true;
            }
            return false;
        }));
    return true;
}
#endif
