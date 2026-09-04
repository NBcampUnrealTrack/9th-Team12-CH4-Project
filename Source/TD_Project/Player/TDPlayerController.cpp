#include "Player/TDPlayerController.h"

#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Character/TDCharacterBase.h"
#include "Combat/TDCombatStatics.h"
#include "Core/TDGameInstance.h"
#include "Game/TDGameMode.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Items/TDInventoryComponent.h"
#include "Market/TDMarketSubsystem.h"
#include "Party/TDPartyComponent.h"
#include "Player/TDPlayerState.h"
#include "Settings/TDChatSettings.h"
#include "Stats/TDProgressionComponent.h"
#include "EngineUtils.h"
#include "World/TDTreasureChest.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDDialogueRow.h"
#include "Engine/DataTable.h"
#include "Quest/TDQuestComponent.h"
#include "World/TDNPCBase.h"

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

void ATDPlayerController::ServerDebugGiveItem_Implementation(FName ItemId, int32 Count)
{
#if !UE_BUILD_SHIPPING
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
	if (ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr)
	{
		GameMode->RespawnPlayer(this);
	}
}

void ATDPlayerController::ServerDebugQuickStart_Implementation(int32 SlotIndex)
{
#if !UE_BUILD_SHIPPING
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
			QuestComponent->AcceptQuest(
				Action.QuestId);

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
			QuestComponent->TurnInQuest(
				Action.QuestId);

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

void ATDPlayerController::ClientMarketSearchResult_Implementation(
	const TArray<FTDMarketListing>& Listings)
{
	OnMarketSearchResult.Broadcast(Listings);

	UE_LOG(LogTemp, Log, TEXT("거래소 검색 결과 %d건"), Listings.Num());
}
