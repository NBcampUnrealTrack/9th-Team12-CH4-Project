#include "Player/TDPlayerController.h"
#include "UI/Settings/TDUISettings.h"
#include "UI/TEST/TDLoginWidget.h"
#include "UI/Core/TDUIManagerSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Party/TDPartyComponent.h"
#include "Player/TDPlayerState.h"
#include "Core/TDAccountSubSystem.h"
#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Engine/GameInstance.h"
#include "Items/TDInventoryComponent.h"
#include "Items/TDItemUseComponent.h"
#include "Items/TDQuickSlotComponent.h"
#include "Stats/TDProgressionComponent.h"
#include "Quest/TDPersonalWorldStateComponent.h"
#include "Quest/TDQuestComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Save/Backend/TDBackendSaveSubsystem.h"

namespace
{
UTDBackendSaveSubsystem* Backend(ATDPlayerController* PC)
{
    auto* Service=PC->GetGameInstance()?PC->GetGameInstance()->GetSubsystem<UTDBackendSaveSubsystem>():nullptr;
    return Service&&Service->IsEnabled()?Service:nullptr;
}
}

void ATDPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ATDPlayerController, DummyAccountId, COND_OwnerOnly);
}

void ATDPlayerController::ServerSelectCharacter_Implementation(int32 SlotIndex)
{
    if(auto* Service=Backend(this)){Service->Select(this,SlotIndex);return;}
    ATDPlayerState* State = GetPlayerState<ATDPlayerState>();
    UTDAccountSubSystem* Store = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDAccountSubSystem>() : nullptr;
    FTDDummyCharacterRecord Record;
    if (!State || !Store || !IsLoggedIn() || State->HasSelectedCharacter()
        || !State->GetCharacterSlots().IsValidIndex(SlotIndex) || !Store->LoadCharacter(State, SlotIndex, Record))
    {
        ClientAccountMessage(TEXT("캐릭터를 불러오지 못했습니다. 로그인과 슬롯 번호를 확인하세요."));
        return;
    }
    ApplyAccountRecord(Record,SlotIndex);
}

void ATDPlayerController::ApplyAccountRecord(const FTDDummyCharacterRecord& Record,int32 SlotIndex)
{
    auto* State=GetPlayerState<ATDPlayerState>();
    if(!State||!State->GetCharacterSlots().IsValidIndex(SlotIndex))return;
    const FTDPlayerSaveData& Data = Record.Data;
    State->GetProgressionComponent()->ReadSaveData(Data);
    // 기본 선택 함수가 경험치를 레벨 시작점으로 덮지 않도록 복원된 레벨과 목록을 맞춘다.
    TArray<FTDCharacterSummary> Slots = State->GetCharacterSlots();
    Slots[SlotIndex].Level = State->GetProgressionComponent()->GetLevel();
    State->SetCharacterSlots(MoveTemp(Slots));
    State->GetInventoryComponent()->ReadSaveData(Data);
    State->GetItemUseComponent()->ReadSaveData(Data);
    State->GetQuickSlotComponent()->ReadSaveData(Data.QuickSlots);
    State->GetPersonalWorldStateComponent()->ReadSaveData(Data);
    State->GetQuestComponent()->ReadSaveData(Data);
    State->SetSavedVitalRatios(Data.HealthRatio, Data.ManaRatio);
    if (Data.LastZoneId.IsValid()) State->SetCurrentZoneId(Data.LastZoneId);
    // 캐릭터 선택/스폰은 기본 클래스의 기존 공개 경로를 그대로 사용한다.
    if (!State->SelectCharacter(SlotIndex))
    {
        ClientAccountMessage(TEXT("캐릭터 선택에 실패했습니다."));
        return;
    }
    DummyCharacterId = Record.CharacterId;
    if (APawn* CharacterPawn = GetPawn())
    {
        // 기존 GameMode로 존 시작점에 스폰한 뒤, 저장된 좌표가 있을 때만 이동한다.
        if (Data.SaveVersion >= 2 && Data.bHasSavedLocation)
        {
            const bool bValidLocation = !Data.LastLocation.ContainsNaN()
                && Data.LastZoneId.IsValid() && State->GetCurrentZoneId() == Data.LastZoneId;
            if (bValidLocation && CharacterPawn->TeleportTo(Data.LastLocation, CharacterPawn->GetActorRotation()))
            {
                if (UPawnMovementComponent* Movement = CharacterPawn->GetMovementComponent()) Movement->StopMovementImmediately();
                CharacterPawn->ForceNetUpdate();
            }
            else UE_LOG(LogTemp, Warning, TEXT("[Account] 저장 위치를 적용할 수 없어 존 시작점을 사용합니다: %s"), *Data.LastLocation.ToString());
        }
        if (State->GetCurrentZoneId().IsValid()) State->SetCurrentZoneId(State->GetCurrentZoneId());
        ClientAccountMessage(TEXT("월드 입장 완료"));
    }
    else ClientAccountMessage(TEXT("월드 스폰에 실패했습니다. GameMode와 PlayerStart를 확인하세요."));
}

void ATDPlayerController::ServerLogin_Implementation(const FString& LoginId, const FString& Password)
{
    if(auto* Service=Backend(this)){Service->Login(this,LoginId,Password);return;}

    ATDPlayerState* State = GetPlayerState<ATDPlayerState>();
    if (!State) return;
    if (State->HasSelectedCharacter() || IsLoggedIn())
    {
        ClientAccountMessage(TEXT("이미 로그인한 상태입니다."));
        return;
    }
    UTDAccountSubSystem* Store = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDAccountSubSystem>() : nullptr;
    FString Error;
    FGuid AccountId;
    if (!Store || !Store->Login(State, LoginId, Password, AccountId, Error))
    {
        ClientAccountMessage(Error.IsEmpty() ? TEXT("더미 저장소를 사용할 수 없습니다.") : Error);
        return;
    }
    DummyAccountId = AccountId;
    State->SetCharacterSlots(Store->ListCharacters(State));
    ClientAccountMessage(TEXT("로그인했습니다. 캐릭터를 선택하세요."));
    ForceNetUpdate();
}

void ATDPlayerController::ClientAccountMessage_Implementation(const FString& Message)
{
    DummyMessage = Message;
    ++AccountReplySerial;
    AccountReplyAction = NAME_None;
    bAccountActionSuccessful = false;
    UE_LOG(LogTemp, Log, TEXT("[DummyAccount] %s"), *Message);
}

bool ATDPlayerController::SaveCharacter()
{
    if(auto* Service=Backend(this))return Service->Save(this);
    ATDPlayerState* State = GetPlayerState<ATDPlayerState>();
    if (!State || !HasAuthority() || !State->HasSelectedCharacter() || !DummyCharacterId.IsValid()) return false;
    UTDAccountSubSystem* Store = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDAccountSubSystem>() : nullptr;
    if (!Store) return false;
    FTDPlayerSaveData Data;
    if(!CaptureAccountData(Data))return false;
    const bool bSaved = Store->SaveCharacter(State, DummyCharacterId, Data);
    if (bSaved) State->SetCharacterSlots(Store->ListCharacters(State));
    return bSaved;
}

bool ATDPlayerController::CaptureAccountData(FTDPlayerSaveData& Data) const
{
    auto* State=GetPlayerState<ATDPlayerState>();
    if(!HasAuthority()||!State||!State->HasSelectedCharacter()||!DummyCharacterId.IsValid())return false;
    State->GetProgressionComponent()->WriteSaveData(Data);
    State->GetInventoryComponent()->WriteSaveData(Data);
    State->GetItemUseComponent()->WriteSaveData(Data);
    State->GetQuickSlotComponent()->WriteSaveData(Data.QuickSlots);
    State->GetPersonalWorldStateComponent()->WriteSaveData(Data);
    State->GetQuestComponent()->WriteSaveData(Data);
    Data.LastZoneId = State->GetCurrentZoneId();
    if (const APawn* CharacterPawn = GetPawn())
    {
        Data.LastLocation = CharacterPawn->GetActorLocation();
        Data.bHasSavedLocation = Data.LastZoneId.IsValid() && !Data.LastLocation.ContainsNaN();
    }
    const float MaxHealth = State->GetAbilitySystemComponent()->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute());
    const float MaxMana = State->GetAbilitySystemComponent()->GetNumericAttribute(UTDAttributeSet::GetMaxManaAttribute());
    Data.HealthRatio = MaxHealth > 0.f ? FMath::Clamp(State->GetAbilitySystemComponent()->GetNumericAttribute(UTDAttributeSet::GetHealthAttribute()) / MaxHealth, 0.f, 1.f) : 1.f;
    Data.ManaRatio = MaxMana > 0.f ? FMath::Clamp(State->GetAbilitySystemComponent()->GetNumericAttribute(UTDAttributeSet::GetManaAttribute()) / MaxMana, 0.f, 1.f) : 1.f;
    return true;
}

void ATDPlayerController::ServerSaveCharacter_Implementation()
{
    if(auto* Service=Backend(this)){if(!Service->Save(this))ClientAccountMessage(TEXT("저장 요청을 시작할 수 없습니다."));return;}
    ClientAccountMessage(SaveCharacter() ? TEXT("더미 메모리에 저장했습니다.") : TEXT("저장할 더미 캐릭터가 없습니다."));
}

void ATDPlayerController::Destroyed()
{
    // APlayerController::Destroyed destroys/unpossesses the Pawn before EndPlay.
    // Capture position and component data while both are still available.
    if(HasAuthority())if(auto* Service=Backend(this))Service->Disconnect(this);
    Super::Destroyed();
}

void ATDPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if(HasAuthority())if(auto* Service=Backend(this))Service->Disconnect(this);
    if (HasAuthority() && GetGameInstance())
    {
        if (UTDAccountSubSystem* Store = GetGameInstance()->GetSubsystem<UTDAccountSubSystem>()) Store->Logout(GetPlayerState<ATDPlayerState>());
    }
    Super::EndPlay(EndPlayReason);
}

void ATDPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController() && bStartAccountFlowOnBeginPlay)
    {
        // 기존 UI 테스트 맵의 즉시 캐릭터 선택이 먼저 끝나도록 다음 틱에서 검사한다.
        GetWorldTimerManager().SetTimerForNextTick(this, &ATDPlayerController::TDLoginScreen);
    }
}

void ATDPlayerController::TDLoginScreen()
{
    if (ULocalPlayer* Local = GetLocalPlayer())
    {
        TSubclassOf<UTDLoginWidget> WidgetClass = DummyLoginWidgetClass;
        if (!WidgetClass)
        {
            WidgetClass = GetDefault<UTDUISettings>()->LoginWidgetClass.LoadSynchronous();
        }
        if (WidgetClass) Local->GetSubsystem<UTDUIManagerSubsystem>()->StartAccountFlow(WidgetClass);
    }
}

void ATDPlayerController::TDSave()
{
    ServerSaveCharacter();
}

void ATDPlayerController::TDCharacterSelect()
{
    ServerLeaveCharacter(false);
}

void ATDPlayerController::TDLogout()
{
    ServerLeaveCharacter(true);
}

void ATDPlayerController::TDQuitGame()
{
    // 종료를 요청할 수 있는 것은 자기 화면의 주인뿐이다.
    if (!IsLocalController())
    {
        return;
    }

    // 캐릭터를 고른 상태에서만 정리할 것이 있다. 로그인 화면에서는 그냥 닫는다.
    const ATDPlayerState* State = GetPlayerState<ATDPlayerState>();
    if (State != nullptr && State->HasSelectedCharacter())
    {
        ServerLeaveCharacter(true);
    }

    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}

void ATDPlayerController::ServerLeaveCharacter_Implementation(bool bLogout)
{
    if(auto* Service=Backend(this)){Service->Leave(this,bLogout);return;}
    ATDPlayerState* Previous = GetPlayerState<ATDPlayerState>();
    UTDAccountSubSystem* Store = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDAccountSubSystem>() : nullptr;
    if (!Previous || !Store || !IsLoggedIn()) return;
    if (!Previous->HasSelectedCharacter())
    {
        if (bLogout)
        {
            Store->Logout(Previous);
            DummyAccountId.Invalidate();
            Previous->SetCharacterSlots({});
        }
        ClientAccountMessage(bLogout ? TEXT("로그아웃했습니다.") : TEXT("캐릭터를 선택하세요."));
        ForceNetUpdate();
        return;
    }

    // 원본 PlayerState의 선택 플래그를 우회해서 고치지 않는다.
    // 새 상태를 먼저 준비하므로 실패해도 기존 캐릭터와 인증을 유지한다.
    InitPlayerState();
    ATDPlayerState* Next = GetPlayerState<ATDPlayerState>();
    if (!Next || Next == Previous || !Store->TransferSession(Previous, Next))
    {
        SetPlayerState(Previous);
        if (Next && Next != Previous) Next->Destroy();
        ClientAccountMessage(TEXT("캐릭터 선택 화면으로 돌아가지 못했습니다."));
        return;
    }
    Next->SetPlayerId(Previous->GetPlayerId());
    Next->SetUniqueId(Previous->GetUniqueId());
    if (Previous->GetPartyComponent()) Previous->GetPartyComponent()->ServerLeaveParty();
    APawn* CharacterPawn = GetPawn();
    UnPossess();
    if (CharacterPawn) CharacterPawn->Destroy();
    Previous->Destroy();
    DummyCharacterId.Invalidate();
    if (bLogout)
    {
        Store->Logout(Next);
        DummyAccountId.Invalidate();
    }
    else Next->SetCharacterSlots(Store->ListCharacters(Next));
    Next->ForceNetUpdate();
    ForceNetUpdate();
    ClientAccountMessage(bLogout ? TEXT("로그아웃했습니다.") : TEXT("캐릭터를 선택하세요."));
}

void ATDPlayerController::ServerRegisterAccount_Implementation(const FString& LoginId, const FString& Password)
{
    if(auto* Service=Backend(this)){Service->Login(this,LoginId,Password,true);return;}
    if (FPlatformTime::Seconds() < NextAccountMutationTime)
    { ClientAccountActionResult(TEXT("Register"), false, TEXT("잠시 후 다시 시도해 주세요.")); return; }
    NextAccountMutationTime = FPlatformTime::Seconds() + 1.0;
    auto* Store = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDAccountSubSystem>() : nullptr;
    FString Error;
    const bool bSuccess = Store && Store->RegisterAccount(GetPlayerState<ATDPlayerState>(), LoginId, Password, Error);
    ClientAccountActionResult(TEXT("Register"), bSuccess, bSuccess ? TEXT("가입했습니다. 로그인해 주세요.") : (Error.IsEmpty() ? TEXT("계정 서비스를 사용할 수 없습니다.") : Error));
}

void ATDPlayerController::ServerCreateCharacter_Implementation(const FString& Name, FName ClassId)
{
    if(auto* Service=Backend(this)){Service->Create(this,Name,ClassId);return;}
    if (FPlatformTime::Seconds() < NextAccountMutationTime)
    { ClientAccountActionResult(TEXT("Create"), false, TEXT("잠시 후 다시 시도해 주세요.")); return; }
    NextAccountMutationTime = FPlatformTime::Seconds() + 1.0;
    auto* Store = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDAccountSubSystem>() : nullptr;
    auto* State = GetPlayerState<ATDPlayerState>();
    FString Error;
    const bool bSuccess = Store && Store->CreateCharacter(State, Name, ClassId, Error);
    if (bSuccess) { State->SetCharacterSlots(Store->ListCharacters(State)); State->ForceNetUpdate(); }
    ClientAccountActionResult(TEXT("Create"), bSuccess, bSuccess ? TEXT("캐릭터를 생성했습니다. 목록에서 선택해 주세요.") : (Error.IsEmpty() ? TEXT("캐릭터를 생성하지 못했습니다.") : Error));
}

void ATDPlayerController::ClientAccountActionResult_Implementation(FName Action, bool bSuccess, const FString& Message)
{
    ClientAccountMessage_Implementation(Message);
    AccountReplyAction = Action;
    bAccountActionSuccessful = bSuccess;
}

void ATDPlayerController::ServerDeleteCharacter_Implementation(const FGuid& CharacterId)
{
    if(auto* Service=Backend(this)){Service->Delete(this,CharacterId);return;}
    if (FPlatformTime::Seconds() < NextAccountMutationTime)
    { ClientAccountActionResult(TEXT("Delete"), false, TEXT("잠시 후 다시 시도해 주세요.")); return; }
    NextAccountMutationTime = FPlatformTime::Seconds() + 1.0;
    auto* Store = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDAccountSubSystem>() : nullptr;
    auto* State = GetPlayerState<ATDPlayerState>();
    FString Error;
    const bool bSuccess = Store && Store->DeleteCharacter(State, CharacterId, Error);
    if (bSuccess) { State->SetCharacterSlots(Store->ListCharacters(State)); State->ForceNetUpdate(); }
    ClientAccountActionResult(TEXT("Delete"), bSuccess, bSuccess ? TEXT("캐릭터를 삭제했습니다.") : (Error.IsEmpty() ? TEXT("삭제 요청을 처리하지 못했습니다.") : Error));
}

void ATDPlayerController::FinishBackendLeave(const TArray<FTDCharacterSummary>& Characters,bool bLogout)
{
    auto* Previous=GetPlayerState<ATDPlayerState>();if(!Previous)return;
    if(Previous->HasSelectedCharacter())
    {
        InitPlayerState();auto* Next=GetPlayerState<ATDPlayerState>();
        if(!Next||Next==Previous){SetPlayerState(Previous);ClientAccountMessage(TEXT("캐릭터 선택 상태를 만들지 못했습니다. 재접속해 주세요."));return;}
        Next->SetPlayerId(Previous->GetPlayerId());Next->SetUniqueId(Previous->GetUniqueId());
        if(Previous->GetPartyComponent())Previous->GetPartyComponent()->ServerLeaveParty();
        APawn* CharacterPawn=GetPawn();UnPossess();if(CharacterPawn)CharacterPawn->Destroy();Previous->Destroy();
    }
    DummyCharacterId.Invalidate();
    if(bLogout)DummyAccountId.Invalidate();
    GetPlayerState<ATDPlayerState>()->SetCharacterSlots(bLogout?TArray<FTDCharacterSummary>():Characters);
    ForceNetUpdate();ClientAccountMessage(bLogout?TEXT("저장 후 로그아웃했습니다."):TEXT("캐릭터를 선택하세요."));
}
