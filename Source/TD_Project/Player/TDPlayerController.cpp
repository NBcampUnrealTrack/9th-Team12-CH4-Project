#include "Player/TDPlayerController.h"
#include "Interaction/TDInteractionFlowComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Character/TDCharacterBase.h"
#include "Combat/TDCombatStatics.h"
#include "Core/TDCheatAccess.h"
#include "Core/TDGameInstance.h"
#include "Game/TDGameMode.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Items/TDInventoryComponent.h"
#include "Market/TDMarketSubsystem.h"
#include "Party/TDPartyComponent.h"
#include "Player/TDPlayerState.h"
#include "Settings/TDChatSettings.h"
#include "Settings/TDZoneSettings.h"
#include "Shop/TDShopStatics.h"
#include "Option/TDOptionServiceComponent.h"
#include "Shop/TDShopServiceComponent.h"
#include "Stats/TDProgressionComponent.h"
#include "EngineUtils.h"
#include "World/TDTreasureChest.h"
#include "World/TDZoneEnvironmentComponent.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDDialogueRow.h"
#include "Engine/DataTable.h"
#include "Quest/TDQuestComponent.h"
#include "World/TDNPCBase.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"
#include "Engine/LocalPlayer.h"
#include "UI/Core/TDUIManagerSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "UI/Settings/TDUISettings.h"

ATDPlayerController::ATDPlayerController()
{
    InteractionFlowComponent = CreateDefaultSubobject<UTDInteractionFlowComponent>(TEXT("InteractionFlowComponent"));
    ShopServiceComponent = CreateDefaultSubobject<UTDShopServiceComponent>(TEXT("ShopServiceComponent"));
    OptionServiceComponent = CreateDefaultSubobject<UTDOptionServiceComponent>(TEXT("OptionServiceComponent"));
	// 로컬 컨트롤러에서만 실제로 동작한다. 서버에 있는 남의 컨트롤러에서는
	// 컴포넌트가 스스로 Tick 을 끈다.
	ZoneEnvironmentComponent = CreateDefaultSubobject<UTDZoneEnvironmentComponent>(
		TEXT("ZoneEnvironmentComponent"));

	// 스폰·텔레포트 직후 주변 셀이 아직 없으면(스트리밍 Critical 이상) 로딩이 끝날 때까지 기다린다.
	// 끄면 바닥이 로드되기 전에 캐릭터가 떨어진다 — 로그인 화면이 다른 곳을 보다가 마을에 스폰할 때
	// 매번 다른 자리에 서던 버그(2026-09-15). 멈칫은 스폰·포탈 이동 순간에만 생긴다.
	bStreamingSourceShouldBlockOnSlowStreaming = true;
}

void ATDPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::LeftAlt, IE_Pressed, this, &ThisClass::ToggleMouseCursor);

    // ESC 는 고정 키다. 설정창에서 바꾸게 두면 잘못 바꿨을 때 설정창을 다시 열 길이 없다.
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::OpenSystemMenu);

    // 나머지 창 단축키는 입력 액션으로 받는다 — 설정창에서 키를 바꿀 수 있게.
    // 창 종류 ↔ 액션 표는 프로젝트 세팅(TD UI > Shortcuts), 기본 키는 IMC_Player.
    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (EnhancedInput == nullptr)
    {
        return;
    }

    for (const TPair<ETDNavMenuType, TSoftObjectPtr<UInputAction>>& Pair : GetDefault<UTDUISettings>()->MenuInputActions)
    {
        if (const UInputAction* Action = Pair.Value.LoadSynchronous())
        {
            EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::HandleMenuAction, Pair.Key);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("창 단축키: '%s' 의 입력 액션이 비어 있거나 불러오지 못했다 — 프로젝트 세팅 TD UI > Shortcuts 확인"),
                *UEnum::GetValueAsString(Pair.Key));
        }
    }
}

void ATDPlayerController::ToggleMouseCursor()
{
    // 커서가 필요한 UI가 떠 있는 동안에는 해당 UI의 입력 모드와 복원 상태를 유지한다.
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UTDUIManagerSubsystem* UI = LocalPlayer->GetSubsystem<UTDUIManagerSubsystem>())
        {
            if (UI->IsChatInputActive() || UI->IsAccountScreenOpen() || UI->HasVisibleGameWindow()) return;
        }
    }
    if (InteractionFlowComponent && (InteractionFlowComponent->IsDialogueActive()
        || InteractionFlowComponent->IsChapterPresentationActive())) return;
    const ATDCharacterBase* ControlledCharacter = Cast<ATDCharacterBase>(GetPawn());
    if (ControlledCharacter && ControlledCharacter->IsDead()) return;
	if (!IsLocalController() || GetPawn() == nullptr)
	{
		return;
	}

	bShowMouseCursor = !bShowMouseCursor;
	if (bShowMouseCursor)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	else
	{
		FInputModeGameOnly Mode;
		Mode.SetConsumeCaptureMouseDown(false);
		SetInputMode(Mode);
	}
}

void ATDPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 새 Pawn 은 BP 기본 카메라 값을 갖고 있다. 존이 바뀐 것이 아니라
	// 알림도 오지 않으므로 여기서 직접 맞춘다.
	if (ZoneEnvironmentComponent != nullptr)
	{
		ZoneEnvironmentComponent->ReapplyCurrentZone();
	}
}

void ATDPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);

	// 클라이언트 경로. 카메라를 실제로 만지는 쪽은 대부분 여기다.
	if (ZoneEnvironmentComponent != nullptr)
	{
		ZoneEnvironmentComponent->ReapplyCurrentZone();
	}
}

void ATDPlayerController::TDConnect(const FString& Address)
{
	// Exec 명령은 서버에도 전달될 수 있으므로 로컬에서만 처리한다.
	if (!IsLocalController())
	{
		return;
	}

	if (UTDGameInstance* TDGameInstance = Cast<UTDGameInstance>(GetGameInstance()))
	{
		TDGameInstance->ConnectToServer(Address);
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("TDConnect: GameInstance 가 UTDGameInstance 가 아니다. Project Settings 를 확인할 것."));
}

void ATDPlayerController::ClientTravelToServer_Implementation(const FString& Address)
{
	if (!IsLocalController() || Address.IsEmpty())
	{
		return;
	}

	ClientTravel(Address, ETravelType::TRAVEL_Absolute);
}

// ── 개발용 치트 ───────────────────────────────────────────
// UFUNCTION 선언은 전처리기로 감쌀 수 없어 항상 남는다.
// 대신 구현부를 막아 배포 빌드에서는 호출해도 아무 일이 일어나지 않는다.

// ── 치트 잠금 ─────────────────────────────────────────────

void ATDPlayerController::TDCheat(const FString& Key)
{
#if !UE_BUILD_SHIPPING
	if (!IsLocalController())
	{
		return;
	}

	// 암호는 서버가 갖고 있다. 여기서는 판단하지 않고 그대로 보낸다.
	ServerEnableCheats(Key);
#endif
}

void ATDPlayerController::ServerEnableCheats_Implementation(const FString& Key)
{
#if !UE_BUILD_SHIPPING
	bCheatsEnabled = TDCheatAccess::VerifyKey(Key);

	UE_LOG(LogTemp, Warning, TEXT("치트 요청: %s — %s"),
		PlayerState != nullptr ? *PlayerState->GetPlayerName() : TEXT("(이름 없음)"),
		bCheatsEnabled ? TEXT("열림") : TEXT("거부"));

	// 콘솔 명령 잠금도 같이 연다. 서버가 허락한 뒤에만 열리는 것이 핵심이다.
	ClientCheatsEnabled(bCheatsEnabled);
#endif
}

void ATDPlayerController::ClientCheatsEnabled_Implementation(bool bEnabled)
{
#if !UE_BUILD_SHIPPING
	TDCheatAccess::SetUnlockedLocally(bEnabled);

	UE_LOG(LogTemp, Warning, TEXT("치트가 %s."),
		bEnabled ? TEXT("열렸다") : TEXT("거부됐다 — 암호를 확인할 것"));
#endif
}

void ATDPlayerController::ServerDebugGiveItem_Implementation(FName ItemId, int32 Count)
{
#if !UE_BUILD_SHIPPING
	if (!bCheatsEnabled) return;
	if (PlayerState == nullptr)
	{
		return;
	}

	if (UTDInventoryComponent* Inventory = PlayerState->FindComponentByClass<UTDInventoryComponent>())
	{
		const bool bAdded = Inventory->AddItem(ItemId, Count);
		UE_LOG(LogTemp, Log, TEXT("[치트] '%s' %d개 지급 %s"),
			*ItemId.ToString(), Count, bAdded ? TEXT("성공") : TEXT("실패"));
	}
#endif
}

void ATDPlayerController::ServerDebugGiveGold_Implementation(int32 Amount)
{
#if !UE_BUILD_SHIPPING
	if (!bCheatsEnabled) return;

	if (PlayerState == nullptr)
	{
		return;
	}

	if (UTDInventoryComponent* Inventory = PlayerState->FindComponentByClass<UTDInventoryComponent>())
	{
		const bool bAdded = Inventory->AddGold(Amount);
		UE_LOG(LogTemp, Log, TEXT("[치트] 골드 %d 지급 %s (보유 %d)"),
			Amount, bAdded ? TEXT("성공") : TEXT("실패"), Inventory->GetGold());
	}
#endif
}

void ATDPlayerController::ServerDebugSetLevel_Implementation(int32 NewLevel)
{
#if !UE_BUILD_SHIPPING
	if (!bCheatsEnabled) return;

	if (PlayerState == nullptr)
	{
		return;
	}

	if (UTDProgressionComponent* Progression = PlayerState->FindComponentByClass<UTDProgressionComponent>())
	{
		const bool bChanged = Progression->SetLevel(NewLevel);
		UE_LOG(LogTemp, Log, TEXT("[치트] 레벨 %d 설정 %s"),
			NewLevel, bChanged ? TEXT("성공") : TEXT("실패"));
	}
#endif
}

void ATDPlayerController::ServerDebugSetClass_Implementation(FName NewClassId)
{
#if !UE_BUILD_SHIPPING
	if (!bCheatsEnabled) return;

	if (PlayerState == nullptr)
	{
		return;
	}

	if (UTDProgressionComponent* Progression = PlayerState->FindComponentByClass<UTDProgressionComponent>())
	{
		const bool bChanged = Progression->SetClassId(NewClassId);
		UE_LOG(LogTemp, Log, TEXT("[치트] 직업 '%s' 설정 %s"),
			*NewClassId.ToString(), bChanged ? TEXT("성공") : TEXT("실패"));
	}
#endif
}

void ATDPlayerController::ServerDebugGiveTestCharacters_Implementation()
{
#if !UE_BUILD_SHIPPING
	if (!bCheatsEnabled) return;

	ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	if (TDPlayerState == nullptr)
	{
		return;
	}

	// ClassId 는 DT_ClassGrowth 에 실제로 있는 값이어야 성장이 붙는다.
	// 없으면 "Default 성장만 적용된다" 경고가 나오는데, 그것도 정상 동작 확인에 쓸 수 있다.
	TArray<FTDCharacterSummary> Slots;

	FTDCharacterSummary& First = Slots.AddDefaulted_GetRef();
	First.CharacterName = TEXT("테스트A");
	First.ClassId = FName(TEXT("Warrior"));
	First.Level = 25;

	FTDCharacterSummary& Second = Slots.AddDefaulted_GetRef();
	Second.CharacterName = TEXT("테스트B");
	Second.ClassId = FName(TEXT("Mage"));
	Second.Level = 12;

	FTDCharacterSummary& Third = Slots.AddDefaulted_GetRef();
	Third.CharacterName = TEXT("테스트C");
	Third.ClassId = FName(TEXT("Archer"));
	Third.Level = 1;

	TDPlayerState->SetCharacterSlots(MoveTemp(Slots));

	UE_LOG(LogTemp, Log, TEXT("[치트] 테스트 캐릭터 3개를 넣었다. (%s)"), *TDPlayerState->GetPlayerName());
#endif
}

void ATDPlayerController::ServerDebugAddExp_Implementation(int32 Amount)
{
#if !UE_BUILD_SHIPPING
	if (!bCheatsEnabled) return;

	if (PlayerState == nullptr)
	{
		return;
	}

	UTDProgressionComponent* Progression = PlayerState->FindComponentByClass<UTDProgressionComponent>();
	if (Progression == nullptr)
	{
		return;
	}

	Progression->AddExp(Amount);

	UE_LOG(LogTemp, Log, TEXT("[치트] 경험치 %d 지급. Level %d, 누적 %d, 다음까지 %d (%.0f%%)"),
		Amount,
		Progression->GetLevel(),
		Progression->GetExp(),
		Progression->GetExpToNextLevel(),
		Progression->GetLevelProgress() * 100.f);
#endif
}

void ATDPlayerController::ServerDebugDamage_Implementation(float Amount)
{
#if !UE_BUILD_SHIPPING
	if (!bCheatsEnabled) return;

	// AController 에 이미 Character 멤버가 있어 그 이름은 쓸 수 없다(C4458).
	ATDCharacterBase* TargetCharacter = Cast<ATDCharacterBase>(GetPawn());
	if (TargetCharacter == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[치트] 조종 중인 캐릭터가 없다. TD.SelectCharacter 로 먼저 스폰할 것."));
		return;
	}

	UTDCombatStatics::ApplyRawDamage(TargetCharacter, Amount);

	// 결과를 함께 찍는다. 요청만 보내고 끝나면 실제로 깎였는지 알 수 없다.
	if (const UAbilitySystemComponent* ASC = TargetCharacter->GetAbilitySystemComponent())
	{
		UE_LOG(LogTemp, Log, TEXT("[치트] %.0f 피해. 체력 %.1f / %.1f%s"),
			Amount,
			ASC->GetNumericAttribute(UTDAttributeSet::GetHealthAttribute()),
			ASC->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute()),
			TargetCharacter->IsDead() ? TEXT(" [사망]") : TEXT(""));
	}
#endif
}

void ATDPlayerController::ServerDebugTravelToZone_Implementation(FGameplayTag TargetZoneId, FName EntryName)
{
#if !UE_BUILD_SHIPPING
	if (!bCheatsEnabled) return;

	ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr;
	if (GameMode == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[치트] GameMode 를 찾지 못했다."));
		return;
	}

	// 거부 사유는 RequestZoneTravel 이 직접 로그로 남긴다. 여기서는 결과만 요약한다.
	const ETDZoneTravelResult Result = GameMode->RequestZoneTravel(this, TargetZoneId, EntryName);

	UE_LOG(LogTemp, Log, TEXT("[치트] 존 이동 %s — '%s' (사유 %d)"),
		Result == ETDZoneTravelResult::Success ? TEXT("성공") : TEXT("거부됨"),
		*TargetZoneId.ToString(), static_cast<int32>(Result));
#endif
}

void ATDPlayerController::ServerRequestUnstuck_Implementation()
{
	ATDGameMode* GameMode = GetWorld() != nullptr
		? GetWorld()->GetAuthGameMode<ATDGameMode>()
		: nullptr;

	if (GameMode == nullptr)
	{
		return;
	}

	// 죽어 있으면 부활 흐름이 따로 있다. 시체를 마을로 옮기면 그쪽과 엉킨다.
	const ATDCharacterBase* MyCharacter = Cast<ATDCharacterBase>(GetPawn());
	if (MyCharacter == nullptr || MyCharacter->IsDead())
	{
		return;
	}

	// 연타 차단. 셀 로딩을 기다리는 중에 텔레포트가 겹치면 좋을 것이 없다.
	const double Now = GetWorld()->GetTimeSeconds();
	const double Remaining = (LastUnstuckTime + UnstuckCooldown) - Now;

	if (Remaining > 0.0)
	{
		GameMode->SendSystemMessage(this, ETDChatChannel::System,
			FString::Printf(TEXT("%.0f초 후에 다시 사용할 수 있습니다."), FMath::CeilToFloat(Remaining)));
		return;
	}

	const FGameplayTag TownZone = GetDefault<UTDZoneSettings>()->DefaultStartZone;
	if (!TownZone.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("끼임 탈출: 프로젝트 세팅 TD > Zone 의 DefaultStartZone 이 비어 있다."));
		return;
	}

	const ETDZoneTravelResult Result = GameMode->RequestZoneTravel(this, TownZone, NAME_None);

	// 성공했을 때만 쿨다운을 건다. 이동하지 못했는데 기다리게 하면
	// 정말 끼인 사람이 20초를 갇힌 채로 보내게 된다.
	if (Result == ETDZoneTravelResult::Success)
	{
		LastUnstuckTime = Now;
	}

	// 눌렀는데 아무 반응이 없으면 버튼이 고장난 것으로 보인다. 결과를 반드시 알린다.
	GameMode->SendSystemMessage(this, ETDChatChannel::System,
		Result == ETDZoneTravelResult::Success
			? TEXT("마을로 돌아왔습니다.")
			: TEXT("지금은 이동할 수 없습니다. 잠시 후 다시 시도해 주세요."));
}

void ATDPlayerController::ClientZoneTravelFailed_Implementation(FGameplayTag TargetZoneId,
	ETDZoneTravelResult Reason)
{
	// 문구는 만들지 않는다. UI 가 이 델리게이트를 받아 자기 형식으로 표시한다.
	OnZoneTravelFailed.Broadcast(TargetZoneId, Reason);

	// UI 가 붙기 전까지는 로그로만 확인한다.
	UE_LOG(LogTemp, Log, TEXT("존 이동 거부됨: '%s' (사유 %d)"),
		*TargetZoneId.ToString(), static_cast<int32>(Reason));
}

void ATDPlayerController::ServerDebugPartyExp_Implementation(int32 BaseAmount)
{
#if !UE_BUILD_SHIPPING
	if (!bCheatsEnabled) return;

	const ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	UTDPartyComponent* Party = TDPlayerState ? TDPlayerState->GetPartyComponent() : nullptr;

	if (Party == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[치트] PartyComponent 를 찾지 못했다."));
		return;
	}

	const int32 Awarded = Party->AwardKillExp(BaseAmount);

	UE_LOG(LogTemp, Log,
		TEXT("[치트] 처치 경험치 %d 분배 → %d명이 받았다 (보너스 +%.0f%%, 존 '%s' 기준)"),
		BaseAmount, Awarded, Party->GetExpBonusRate() * 100.f,
		*TDPlayerState->GetCurrentZoneId().ToString());
#endif
}

void ATDPlayerController::ServerRequestRespawn_Implementation()
{
	// 치트가 아니므로 Shipping 가드를 두지 않는다.
	bool bSucceeded = false;
	if (ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr)
	{
		bSucceeded = GameMode->RespawnPlayer(this);
	}
	ClientRespawnRequestResult(bSucceeded);
}

void ATDPlayerController::ClientRespawnRequestResult_Implementation(bool bSucceeded)
{
	OnRespawnRequestResult.Broadcast(bSucceeded);
}

void ATDPlayerController::ServerDebugQuickStart_Implementation(int32 SlotIndex)
{
#if !UE_BUILD_SHIPPING
	if (!bCheatsEnabled) return;

	ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	if (TDPlayerState == nullptr)
	{
		return;
	}

	// 지급과 선택이 같은 함수 안에서 순서대로 일어난다. 서버에서 직접 부르므로
	// 두 RPC 사이에 다른 것이 끼어들 여지가 없다.
	ServerDebugGiveTestCharacters_Implementation();

	const bool bSelected = TDPlayerState->SelectCharacter(SlotIndex);

	UE_LOG(LogTemp, Log, TEXT("[치트] 빠른 시작: %d번 캐릭터 선택 %s"),
		SlotIndex, bSelected ? TEXT("성공") : TEXT("실패"));

	// 선택 직후에 스킬을 전부 1레벨로 찍어 둔다. 액티브는 찍지 않으면 나가지 않아서
	// 빠른 시작을 쓴 뒤에도 매번 TD.SkillUp 을 세 번 쳐야 했다.
	// 직업이 정해진 뒤여야 하므로 SelectCharacter 다음이다.
	if (bSelected)
	{
		ServerDebugLearnSkills_Implementation(1);
	}
#endif
}

void ATDPlayerController::ServerDebugLearnSkills_Implementation(int32 SkillLevel)
{
#if !UE_BUILD_SHIPPING
	if (!bCheatsEnabled) return;

	ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	UTDProgressionComponent* Progression =
		TDPlayerState ? TDPlayerState->GetProgressionComponent() : nullptr;

	if (Progression == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[치트] 스킬 습득: ProgressionComponent 를 찾지 못했다. 캐릭터를 먼저 선택할 것."));
		return;
	}

	const int32 ChangedCount = Progression->DebugLearnAllSkills(SkillLevel);

	UE_LOG(LogTemp, Log, TEXT("[치트] 직업 '%s' 의 스킬 %d개를 레벨 %d 로 맞췄다."),
		*Progression->GetClassId().ToString(), ChangedCount, SkillLevel);
#endif
}

void ATDPlayerController::ClientChestClaimed_Implementation(
	FName ChestId,
	float DisappearDelay)
{
	if (!IsLocalController() || ChestId.IsNone())
	{
		return;
	}

	// 서버와 클라이언트의 상자 Actor는 서로 다른 객체다.
	// Actor 포인터 대신 영구 ID를 받아 이 클라이언트 월드의 상자를 찾는다.
	for (TActorIterator<ATDTreasureChest> It(GetWorld()); It; ++It)
	{
		ATDTreasureChest* Chest = *It;

		if (IsValid(Chest)
			&& Chest->GetChestId() == ChestId)
		{
			Chest->PlayClaimedPresentation(DisappearDelay);
			return;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("ClientChestClaimed: 클라이언트 월드에서 상자 '%s'를 찾지 못했다."),
		*ChestId.ToString());
}

void ATDPlayerController::ClientInteractionFailed_Implementation(
	FName ObjectId,
	ETDInteractionFailureReason Reason)
{
	if (!IsLocalController())
	{
		return;
	}

	OnInteractionFailed.Broadcast(ObjectId, Reason);

	// UI가 만들어지기 전에는 로그로 확인한다.
	UE_LOG(LogTemp, Log,
		TEXT("상호작용 실패: Object='%s', Reason=%d"),
		*ObjectId.ToString(),
		static_cast<int32>(Reason));
}

void ATDPlayerController::BeginDialogueFromNPC(
	ATDNPCBase* NPC,
	UDataTable* DialogueTable,
	FName StartRow)
{
	if (!HasAuthority()
		|| !IsValid(NPC)
		|| DialogueTable == nullptr
		|| StartRow.IsNone())
	{
		return;
	}

	const FTDDialogueRow* FirstRow =
		DialogueTable->FindRow<FTDDialogueRow>(
			StartRow,
			TEXT("BeginDialogueFromNPC"),
			false);

	if (FirstRow == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("대화 시작 실패: DT_Dialogue에 '%s' 행이 없다."),
			*StartRow.ToString());
		return;
	}

	if (ActiveDialogueSessionId != 0)
	{
		EndDialogueSession();
	}

	++DialogueSessionCounter;

	if (DialogueSessionCounter <= 0)
	{
		DialogueSessionCounter = 1;
	}

	ActiveDialogueSessionId =
		DialogueSessionCounter;

	ActiveDialogueNPC = NPC;
	ActiveDialogueTable = DialogueTable;
	ActiveDialogueRow = StartRow;

	SendCurrentDialogueLine();
}

void ATDPlayerController::SendCurrentDialogueLine()
{
	if (!HasAuthority()
		|| ActiveDialogueSessionId == 0
		|| !IsValid(ActiveDialogueNPC)
		|| ActiveDialogueTable == nullptr
		|| ActiveDialogueRow.IsNone())
	{
		EndDialogueSession();
		return;
	}

	const FTDDialogueRow* Row =
		ActiveDialogueTable->FindRow<FTDDialogueRow>(
			ActiveDialogueRow,
			TEXT("SendCurrentDialogueLine"),
			false);

	if (Row == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("대화 행 '%s'를 찾지 못했다."),
			*ActiveDialogueRow.ToString());

		EndDialogueSession();
		return;
	}

	FTDDialogueLineView View;
	View.bPlayerSpeaker = Row->bPlayerSpeaker;
	View.DialogueText = Row->DialogueText;

	if (!Row->SpeakerNameOverride.IsEmpty())
	{
		View.SpeakerName =
			Row->SpeakerNameOverride;
	}
	else if (Row->bPlayerSpeaker)
	{
		const ATDPlayerState* TDPlayerState =
			GetPlayerState<ATDPlayerState>();

		const FString PlayerDisplayName =
			TDPlayerState
				? TDPlayerState->GetPlayerName()
				: FString();

		View.SpeakerName =
			PlayerDisplayName.IsEmpty()
				? NSLOCTEXT(
					"TDDialogue",
					"DefaultPlayerName",
					"플레이어")
				: FText::FromString(PlayerDisplayName);
	}
	else
	{
		View.SpeakerName =
			ActiveDialogueNPC->GetNPCDisplayName();
	}

	if (!Row->PortraitOverride.IsNull())
	{
		View.Portrait =
			Row->PortraitOverride;
	}
	else if (!Row->bPlayerSpeaker)
	{
		View.Portrait =
			ActiveDialogueNPC->GetNPCPortrait();
	}

	View.ContinueButtonText =
		Row->ContinueButtonText.IsEmpty()
			? NSLOCTEXT(
				"TDDialogue",
				"Continue",
				"다음")
			: Row->ContinueButtonText;

	ClientShowDialogueLine(
		ActiveDialogueSessionId,
		ActiveDialogueRow,
		View);
}

bool ATDPlayerController::ApplyDialogueAction(
	const FTDDialogueAction& Action)
{
	if (!Action.ActionType.IsValid())
	{
		return true;
	}

	ATDPlayerState* TDPlayerState =
		GetPlayerState<ATDPlayerState>();

	UTDQuestComponent* QuestComponent =
		TDPlayerState
			? TDPlayerState->GetQuestComponent()
			: nullptr;

	if (QuestComponent == nullptr)
	{
		ClientQuestActionResult(
			Action.QuestId,
			ETDQuestActionResult::InvalidDefinition);

		return false;
	}

	if (Action.ActionType ==
		TDTags::Dialogue_Action_AcceptQuest.GetTag())
	{
		const ETDQuestActionResult Result =
			QuestComponent->AcceptQuestAtTarget(
				Action.QuestId,
				ETDQuestTargetType::NPC,
				ActiveDialogueNPC
					? ActiveDialogueNPC->GetNPCId()
					: NAME_None);

		ClientQuestActionResult(
			Action.QuestId,
			Result);

		return Result ==
			ETDQuestActionResult::Success;
	}

	if (Action.ActionType ==
		TDTags::Dialogue_Action_TurnInQuest.GetTag())
	{
		const ETDQuestActionResult Result =
			QuestComponent->TurnInQuestAtTarget(
				Action.QuestId,
				ETDQuestTargetType::NPC,
				ActiveDialogueNPC
					? ActiveDialogueNPC->GetNPCId()
					: NAME_None);

		ClientQuestActionResult(
			Action.QuestId,
			Result);

		return Result ==
			ETDQuestActionResult::Success;
	}

	if (Action.ActionType ==
		TDTags::Dialogue_Action_ReportQuestEvent.GetTag())
	{
		QuestComponent->ReportQuestEvent(
			Action.EventTag,
			FMath::Max(1, Action.EventAmount));

		return true;
	}

	ClientQuestActionResult(
		Action.QuestId,
		ETDQuestActionResult::InvalidDefinition);

	return false;
}

void ATDPlayerController::ServerAdvanceDialogue_Implementation(
	int32 SessionId,
	FName ExpectedCurrentRow)
{
	if (SessionId != ActiveDialogueSessionId
		|| ExpectedCurrentRow != ActiveDialogueRow
		|| !IsValid(ActiveDialogueNPC)
		|| ActiveDialogueTable == nullptr)
	{
		return;
	}

	// 대화 시작 후 너무 멀리 도망갔으면 서버가 종료한다.
	if (!ActiveDialogueNPC
		->IsPlayerWithinDialogueDistance(GetPawn()))
	{
		EndDialogueSession();
		return;
	}

	const FTDDialogueRow* Row =
		ActiveDialogueTable->FindRow<FTDDialogueRow>(
			ActiveDialogueRow,
			TEXT("ServerAdvanceDialogue"),
			false);

	if (Row == nullptr)
	{
		EndDialogueSession();
		return;
	}

	// 보상 지급이나 퀘스트 수락이 실패하면 현재 줄에 그대로 남는다.
	if (!ApplyDialogueAction(
		Row->OnAdvanceAction))
	{
		EndDialogueSession();
		return;
	}

	if (Row->NextRow.IsNone())
	{
		EndDialogueSession();
		return;
	}

	ActiveDialogueRow = Row->NextRow;
	SendCurrentDialogueLine();
}

void ATDPlayerController::ServerCancelDialogue_Implementation(
	int32 SessionId)
{
	if (SessionId != ActiveDialogueSessionId)
	{
		return;
	}

	EndDialogueSession();
}

void ATDPlayerController::EndDialogueSession()
{
	const int32 EndedSessionId =
		ActiveDialogueSessionId;

	ActiveDialogueSessionId = 0;
	ActiveDialogueNPC = nullptr;
	ActiveDialogueTable = nullptr;
	ActiveDialogueRow = NAME_None;

	if (EndedSessionId != 0)
	{
		ClientCloseDialogue(EndedSessionId);
	}
}

void ATDPlayerController::ClientShowDialogueLine_Implementation(
	int32 SessionId,
	FName DialogueRow,
	FTDDialogueLineView Line)
{
	if (!IsLocalController())
	{
		return;
	}

	OnDialogueLineReceived.Broadcast(
		SessionId,
		DialogueRow,
		Line);
}

void ATDPlayerController::ClientCloseDialogue_Implementation(
	int32 SessionId)
{
	if (!IsLocalController())
	{
		return;
	}

	OnDialogueClosed.Broadcast(SessionId);
}

void ATDPlayerController::ClientQuestActionResult_Implementation(
	FName QuestId,
	ETDQuestActionResult Result)
{
	if (!IsLocalController())
	{
		return;
	}

	OnQuestActionResult.Broadcast(
		QuestId,
		Result);

	UE_LOG(LogTemp, Log,
		TEXT("퀘스트 처리 결과: Quest='%s', Result=%d"),
		*QuestId.ToString(),
		static_cast<int32>(Result));
}

// ── 채팅 ──────────────────────────────────────────────────

void ATDPlayerController::ServerSendChat_Implementation(ETDChatChannel Channel,
	const FString& Message, const FString& TargetName)
{
	ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr;
	if (GameMode == nullptr)
	{
		return;
	}

	// 도배 검사를 여기서 하는 이유는 "마지막으로 보낸 시각" 이 이 플레이어의 상태라서다.
	// GameMode 가 들고 있으면 접속자마다 목록을 관리해야 하고, 나갈 때 지우는 것도
	// 잊으면 안 된다. 컨트롤러가 사라지면 이 값도 함께 사라진다.
	const UTDChatSettings* Settings = UTDChatSettings::Get();
	const float Cooldown = Settings ? Settings->SendCooldownSeconds : 0.f;

	if (Cooldown > 0.f)
	{
		// 서버 시각으로만 잰다. 클라이언트가 보낸 값을 믿으면 그대로 조작된다.
		const double Now = FPlatformTime::Seconds();
		if (Now - LastChatSendTime < Cooldown)
		{
			ClientChatSendFailed(ETDChatSendResult::TooFast);
			return;
		}
	}

	const ETDChatSendResult Result = GameMode->RouteChatMessage(this, Channel, Message, TargetName);

	if (Result != ETDChatSendResult::Success)
	{
		ClientChatSendFailed(Result);
		return;
	}

	// 성공한 뒤에 시각을 갱신한다. 거부된 요청까지 쿨다운에 넣으면
	// 오타 한 번에 다음 말까지 막힌다.
	LastChatSendTime = FPlatformTime::Seconds();
}

void ATDPlayerController::ClientReceiveChat_Implementation(ETDChatChannel Channel,
	const FString& SenderName, const FString& Message)
{
	OnChatReceived.Broadcast(Channel, SenderName, Message);

	// UI 가 붙기 전까지는 로그로 확인한다.
	UE_LOG(LogTemp, Log, TEXT("[채팅/%s] %s%s"),
		*UEnum::GetDisplayValueAsText(Channel).ToString(),
		SenderName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("%s: "), *SenderName),
		*Message);
}

void ATDPlayerController::ClientChatSendFailed_Implementation(ETDChatSendResult Reason)
{
	// 문구는 만들지 않는다. UI 가 이 델리게이트를 받아 자기 형식으로 표시한다.
	OnChatSendFailed.Broadcast(Reason);

	UE_LOG(LogTemp, Log, TEXT("채팅 거부됨: %s"),
		*UEnum::GetDisplayValueAsText(Reason).ToString());
}

// ── 거래소 ────────────────────────────────────────────────
// 실제 처리는 UTDMarketSubsystem 이 한다. 여기는 요청을 넘기고 결과를 돌려주는 통로다.

namespace
{
	/** 서버에서만 유효하다. 클라이언트에도 서브시스템은 있지만 매물 목록이 비어 있다. */
	UTDMarketSubsystem* GetMarket(const APlayerController* Controller)
	{
		const UGameInstance* GameInstance = Controller ? Controller->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UTDMarketSubsystem>() : nullptr;
	}
}

void ATDPlayerController::ServerListItem_Implementation(int32 InventorySlot, int32 Price)
{
	UTDMarketSubsystem* Market = GetMarket(this);
	ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();

	if (Market == nullptr || TDPlayerState == nullptr)
	{
		ClientMarketResult(ETDMarketResult::InternalError, 0);
		return;
	}

	int32 NewListingId = 0;
	const ETDMarketResult Result = Market->ListItem(TDPlayerState, InventorySlot, Price, NewListingId);

	ClientMarketResult(Result, NewListingId);
}

void ATDPlayerController::ServerBuyListing_Implementation(int32 ListingId)
{
	UTDMarketSubsystem* Market = GetMarket(this);
	ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();

	if (Market == nullptr || TDPlayerState == nullptr)
	{
		ClientMarketResult(ETDMarketResult::InternalError, ListingId);
		return;
	}

	// 요청한 번호를 그대로 돌려준다. UI 가 어느 줄에 대한 결과인지 알아야 한다.
	ClientMarketResult(Market->BuyListing(TDPlayerState, ListingId), ListingId);
}

void ATDPlayerController::ServerCancelListing_Implementation(int32 ListingId)
{
	UTDMarketSubsystem* Market = GetMarket(this);
	ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();

	if (Market == nullptr || TDPlayerState == nullptr)
	{
		ClientMarketResult(ETDMarketResult::InternalError, ListingId);
		return;
	}

	ClientMarketResult(Market->CancelListing(TDPlayerState, ListingId), ListingId);
}

void ATDPlayerController::ServerSearchListings_Implementation(FName ItemIdFilter, int32 Page)
{
	if (const UTDMarketSubsystem* Market = GetMarket(this))
	{
		ClientMarketSearchResult(Market->Search(ItemIdFilter, Page));
	}
}

void ATDPlayerController::ServerRequestMyListings_Implementation()
{
	const UTDMarketSubsystem* Market = GetMarket(this);
	const ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();

	if (Market == nullptr || TDPlayerState == nullptr)
	{
		return;
	}

	ClientMarketSearchResult(Market->GetListingsBySeller(TDPlayerState->GetPlayerName()));
}

void ATDPlayerController::ClientMarketResult_Implementation(ETDMarketResult Result, int32 ListingId)
{
	OnMarketResult.Broadcast(Result, ListingId);

	UE_LOG(LogTemp, Log, TEXT("거래소 결과: %s (매물 %d)"),
		*UEnum::GetDisplayValueAsText(Result).ToString(), ListingId);
}

// ── NPC 상점 ──────────────────────────────────────────────

void ATDPlayerController::ServerBuyFromShop_Implementation(FName ShopId, FName ItemId, int32 Count)
{
	const UTDShopServiceComponent* ShopService =
		FindComponentByClass<UTDShopServiceComponent>();

	if (!ShopService || !ShopService->IsActiveForShop(ShopId))
	{
		ClientShopResult(ETDShopResult::TooFar, ItemId, Count, 0);
		return;
	}

	ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();

	int32 TotalPrice = 0;
	const ETDShopResult Result = TDPlayerState != nullptr
		? UTDShopStatics::BuyItem(TDPlayerState, ShopId, ItemId, Count, TotalPrice)
		: ETDShopResult::NoCharacterSelected;

	// 무엇에 대한 결과인지 함께 돌려준다. 요청을 연달아 보내면 순서가 섞일 수 있어
	// UI 가 "방금 누른 그것" 인지 판단할 근거가 필요하다.
	ClientShopResult(Result, ItemId, Count, TotalPrice);
}

void ATDPlayerController::ServerSellToShop_Implementation(FName ShopId, int32 InventorySlot, int32 Count)
{
	const UTDShopServiceComponent* ShopService =
		FindComponentByClass<UTDShopServiceComponent>();

	if (!ShopService || !ShopService->IsActiveForShop(ShopId))
	{
		ClientShopResult(ETDShopResult::TooFar, NAME_None, Count, 0);
		return;
	}

	ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();

	// 어느 아이템이었는지 먼저 알아둔다. 팔고 나면 그 칸이 비어 되짚을 수 없다.
	FName SoldItemId;
	if (const UTDInventoryComponent* Inventory =
		TDPlayerState ? TDPlayerState->GetInventoryComponent() : nullptr)
	{
		if (const FTDItemInstance* Found = Inventory->GetItems().FindByPredicate(
			[InventorySlot](const FTDItemInstance& Item) { return Item.SlotIndex == InventorySlot; }))
		{
			SoldItemId = Found->ItemId;
		}
	}

	int32 TotalPrice = 0;
	const ETDShopResult Result = TDPlayerState != nullptr
		? UTDShopStatics::SellItem(TDPlayerState, ShopId, InventorySlot, Count, TotalPrice)
		: ETDShopResult::NoCharacterSelected;

	ClientShopResult(Result, SoldItemId, Count, TotalPrice);
}

void ATDPlayerController::ClientShopResult_Implementation(
	ETDShopResult Result, FName ItemId, int32 Count, int32 TotalPrice)
{
	OnShopResult.Broadcast(Result, ItemId, Count, TotalPrice);

	UE_LOG(LogTemp, Log, TEXT("상점 결과: %s ('%s' %d개, %d골드)"),
		*UEnum::GetDisplayValueAsText(Result).ToString(), *ItemId.ToString(), Count, TotalPrice);
}

void ATDPlayerController::ClientMarketSearchResult_Implementation(
	const TArray<FTDMarketListing>& Listings)
{
	OnMarketSearchResult.Broadcast(Listings);

	UE_LOG(LogTemp, Log, TEXT("거래소 검색 결과 %d건"), Listings.Num());
}

bool ATDPlayerController::HandleNavShortcut(FKey Key)
{
    if (Key == EKeys::Escape) return TryOpenMenu(ETDNavMenuType::System);

    // UI 에 포커스가 있을 때는 입력 액션이 오지 않아 키로 들어온다. 키를 코드에 적어 두면
    // 설정창에서 바꾼 키가 여기서만 안 먹으므로, 각 액션에 **지금 지정된 키** 와 비교한다.
    const UEnhancedInputLocalPlayerSubsystem* Subsystem = GetLocalPlayer()
        ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()) : nullptr;
    if (!Subsystem) return false;

    for (const TPair<ETDNavMenuType, TSoftObjectPtr<UInputAction>>& Pair : GetDefault<UTDUISettings>()->MenuInputActions)
    {
        const UInputAction* Action = Pair.Value.LoadSynchronous();
        if (Action && Subsystem->QueryKeysMappedToAction(Action).Contains(Key))
        {
            return TryOpenMenu(Pair.Key);
        }
    }
    return false;
}

bool ATDPlayerController::TryOpenMenu(ETDNavMenuType MenuType)
{
    if (!IsLocalController()) return false;
    // ESC는 기존 InteractionFlow의 대화 취소가 처리한다. 여기서는 메뉴만 막는다.
    if (MenuType == ETDNavMenuType::System && InteractionFlowComponent
        && InteractionFlowComponent->IsDialogueActive()) return false;
    ULocalPlayer* Local = GetLocalPlayer();
    UTDUIManagerSubsystem* UI = Local ? Local->GetSubsystem<UTDUIManagerSubsystem>() : nullptr;
    if (!UI || UI->IsChatInputActive() || !UI->IsGameplayWindowLayerReady()) return false;
    // 설정창 안의 텍스트 입력에 포커스가 있어도 ESC로 닫을 수 있다.
    // 채팅은 위에서 제외하며, 다른 메뉴의 단축키/텍스트 입력 정책은 유지한다.
    if (MenuType == ETDNavMenuType::System && UI->IsMenuOpen(MenuType))
    {
        UI->RequestMenu(MenuType);
        return true;
    }
    if (FSlateApplication::IsInitialized())
    {
        const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
        if (Focused.IsValid() && Focused->GetTypeAsString().Contains(TEXT("EditableText"))) return false;
    }
    UI->RequestMenu(MenuType);
    return true;
}

void ATDPlayerController::OpenCharacterMenu()
{
    TryOpenMenu(ETDNavMenuType::Character);
}

void ATDPlayerController::OpenInventoryMenu()
{
    TryOpenMenu(ETDNavMenuType::Inventory);
}

void ATDPlayerController::OpenSkillMenu()
{
    TryOpenMenu(ETDNavMenuType::Skill);
}

void ATDPlayerController::OpenQuestMenu()
{
    TryOpenMenu(ETDNavMenuType::Quest);
}

void ATDPlayerController::OpenPartyMenu()
{
    TryOpenMenu(ETDNavMenuType::Party);
}

void ATDPlayerController::OpenSystemMenu()
{
    // ESC 는 두 가지 일을 한다 — 열린 창이 있으면 가장 앞의 것을 닫고,
    // 전부 닫혀 있을 때만 설정창을 연다. 창을 여러 개 띄워 놓고 하나씩 치울 수 있다.
    //
    // 아래 검사는 TryOpenMenu 의 앞부분과 같다. 창을 닫는 판단이 그보다 먼저 와야 해서
    // 여기서 한 번 더 본다 — 대화·채팅 중에는 ESC 가 그쪽 것이다.
    if (!IsLocalController()) return;

    if (InteractionFlowComponent && InteractionFlowComponent->IsDialogueActive()) return;

    ULocalPlayer* Local = GetLocalPlayer();
    UTDUIManagerSubsystem* UI = Local ? Local->GetSubsystem<UTDUIManagerSubsystem>() : nullptr;
    if (!UI || UI->IsChatInputActive() || !UI->IsGameplayWindowLayerReady()) return;

    // 텍스트 입력에 포커스가 있어도 창은 닫힌다. 거래소 검색칸에 커서를 둔 채로도
    // ESC 로 빠져나올 수 있어야 한다(설정창에 대해 원래 있던 규칙과 같은 이유).
    if (UI->CloseTopmostWindow()) return;

    TryOpenMenu(ETDNavMenuType::System);
}
