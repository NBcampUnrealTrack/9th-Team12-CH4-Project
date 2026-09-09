#include "UI/TEST/TDUI_Login_PlayerController.h"
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
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"

void ATDUI_Login_PlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ATDUI_Login_PlayerController, DummyAccountId, COND_OwnerOnly);
}

void ATDUI_Login_PlayerController::ServerSelectCharacter_Implementation(int32 SlotIndex)
{
    ATDPlayerState* State = GetPlayerState<ATDPlayerState>();
    UTDAccountSubSystem* Store = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDAccountSubSystem>() : nullptr;
    FTDDummyCharacterRecord Record;
    if (!State || !Store || !IsLoggedIn() || State->HasSelectedCharacter()
        || !State->GetCharacterSlots().IsValidIndex(SlotIndex) || !Store->LoadCharacter(State, SlotIndex, Record))
    {
        ClientAccountMessage(TEXT("캐릭터를 불러오지 못했습니다. 로그인과 슬롯 번호를 확인하세요."));
        return;
    }
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

void ATDUI_Login_PlayerController::ServerLogin_Implementation(const FString& LoginId, const FString& Password)
{
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

void ATDUI_Login_PlayerController::ClientAccountMessage_Implementation(const FString& Message)
{
    DummyMessage = Message;
    UE_LOG(LogTemp, Log, TEXT("[DummyAccount] %s"), *Message);
}

bool ATDUI_Login_PlayerController::SaveCharacter()
{
    ATDPlayerState* State = GetPlayerState<ATDPlayerState>();
    if (!State || !HasAuthority() || !State->HasSelectedCharacter() || !DummyCharacterId.IsValid()) return false;
    UTDAccountSubSystem* Store = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTDAccountSubSystem>() : nullptr;
    if (!Store) return false;
    FTDPlayerSaveData Data;
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
    const bool bSaved = Store->SaveCharacter(State, DummyCharacterId, Data);
    if (bSaved) State->SetCharacterSlots(Store->ListCharacters(State));
    return bSaved;
}

void ATDUI_Login_PlayerController::ServerSaveCharacter_Implementation()
{
    ClientAccountMessage(SaveCharacter() ? TEXT("더미 메모리에 저장했습니다.") : TEXT("저장할 더미 캐릭터가 없습니다."));
}

void ATDUI_Login_PlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority() && GetGameInstance())
    {
        if (UTDAccountSubSystem* Store = GetGameInstance()->GetSubsystem<UTDAccountSubSystem>()) Store->Logout(GetPlayerState<ATDPlayerState>());
    }
    Super::EndPlay(EndPlayReason);
}

void ATDUI_Login_PlayerController::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    if (IsLocalController())
    {
        // 기존 UI 테스트 맵의 즉시 캐릭터 선택이 먼저 끝나도록 다음 틱에서 검사한다.
        GetWorldTimerManager().SetTimerForNextTick(this, &ATDUI_Login_PlayerController::TDLoginScreen);
    }
#endif
}

void ATDUI_Login_PlayerController::TDLoginScreen()
{
#if !UE_BUILD_SHIPPING
    if (ULocalPlayer* Local = GetLocalPlayer())
    {
        Local->GetSubsystem<UTDUIManagerSubsystem>()->StartAccountFlow(DummyLoginWidgetClass);
    }
#endif
}

void ATDUI_Login_PlayerController::TDSave()
{
    ServerSaveCharacter();
}

void ATDUI_Login_PlayerController::TDCharacterSelect()
{
    ServerLeaveCharacter(false);
}

void ATDUI_Login_PlayerController::TDLogout()
{
    ServerLeaveCharacter(true);
}

void ATDUI_Login_PlayerController::ServerLeaveCharacter_Implementation(bool bLogout)
{
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
