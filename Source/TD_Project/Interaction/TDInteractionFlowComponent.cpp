#include "Interaction/TDInteractionFlowComponent.h"

#include "Character/TDCharacterBase.h"
#include "Components/ActorComponent.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDItemRow.h"
#include "Engine/DataTable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TDDialogueSource.h"
#include "Items/TDInventoryComponent.h"
#include "Items/TDItemUseComponent.h"
#include "Player/TDPlayerState.h"
#include "Quest/TDPersonalWorldStateComponent.h"
#include "Quest/TDQuestComponent.h"
#include "World/TDNPCBase.h"
#include "EngineUtils.h"

UTDInteractionFlowComponent::
UTDInteractionFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

APlayerController*
UTDInteractionFlowComponent::GetOwnerController() const
{
	return Cast<APlayerController>(GetOwner());
}

UTDQuestComponent*
UTDInteractionFlowComponent::GetQuestComponent() const
{
	const APlayerController* Controller =
		GetOwnerController();

	const ATDPlayerState* PlayerState =
		Controller
			? Controller
				->GetPlayerState<ATDPlayerState>()
			: nullptr;

	return PlayerState
		? PlayerState->GetQuestComponent()
		: nullptr;
}

void UTDInteractionFlowComponent::BeginPlay()
{
	Super::BeginPlay();

	TryBindQuestComponent();

	if (BoundQuestComponent == nullptr)
	{
		GetWorld()->GetTimerManager().SetTimer(
			BindRetryTimerHandle,
			this,
			&UTDInteractionFlowComponent::
				TryBindQuestComponent,
			0.25f,
			true);
	}
}

void UTDInteractionFlowComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(
			BindRetryTimerHandle);

		GetWorld()->GetTimerManager().ClearTimer(
			ChapterTimerHandle);
	}

	if (BoundQuestComponent != nullptr)
	{
		BoundQuestComponent
			->OnQuestListChanged
			.RemoveDynamic(
				this,
				&UTDInteractionFlowComponent::HandleQuestListChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void UTDInteractionFlowComponent::TryBindQuestComponent()
{
	if (BoundQuestComponent != nullptr)
	{
		return;
	}

	BoundQuestComponent = GetQuestComponent();

	if (BoundQuestComponent == nullptr)
	{
		return;
	}

	BoundQuestComponent
		->OnQuestListChanged
		.AddUniqueDynamic(
			this,
			&UTDInteractionFlowComponent::HandleQuestListChanged);

	GetWorld()->GetTimerManager().ClearTimer(
		BindRetryTimerHandle);

	if (GetOwner()->HasAuthority())
	{
		TryStartCurrentChapter();
	}
}

void UTDInteractionFlowComponent::
HandleQuestListChanged()
{
	if (GetOwner()->HasAuthority())
	{
		TryStartCurrentChapter();
	}
}

const FTDChapterRow*
UTDInteractionFlowComponent::FindChapterForQuest(
	FName QuestId,
	FName& OutChapterId) const
{
	OutChapterId = NAME_None;

	if (ChapterTable == nullptr
		|| QuestId.IsNone())
	{
		return nullptr;
	}

	const FTDChapterRow* Found = nullptr;

	ChapterTable->ForeachRow<FTDChapterRow>(
		TEXT("FindChapterForQuest"),
		[QuestId, &Found, &OutChapterId](
			const FName& RowName,
			const FTDChapterRow& Row)
		{
			if (Found == nullptr
				&& Row.StartQuestId == QuestId)
			{
				Found = &Row;
				OutChapterId = RowName;
			}
		});

	return Found;
}

void UTDInteractionFlowComponent::
TryStartCurrentChapter()
{
	if (!GetOwner()->HasAuthority()
		|| !ActiveChapterId.IsNone()
		|| BoundQuestComponent == nullptr)
	{
		return;
	}

	FName CurrentMainQuestId = NAME_None;

	for (const FTDQuestViewData& View :
		BoundQuestComponent
			->GetQuestTrackerViews())
	{
		if (View.QuestTypeTag ==
			TDTags::Quest_Type_Main.GetTag())
		{
			CurrentMainQuestId = View.QuestId;
			break;
		}
	}

	if (CurrentMainQuestId.IsNone())
	{
		return;
	}

	FName ChapterId;
	const FTDChapterRow* Chapter =
		FindChapterForQuest(
			CurrentMainQuestId,
			ChapterId);

	if (Chapter == nullptr
		|| ChapterId.IsNone())
	{
		return;
	}

	const APlayerController* Controller =
		GetOwnerController();

	const ATDPlayerState* PlayerState =
		Controller
			? Controller
				->GetPlayerState<ATDPlayerState>()
			: nullptr;

	const UTDPersonalWorldStateComponent* Personal =
		PlayerState
			? PlayerState
				->GetPersonalWorldStateComponent()
			: nullptr;

	if (Personal == nullptr
		|| Personal->HasSeenChapter(ChapterId))
	{
		return;
	}

	if (ActiveDialogueSessionId != 0)
	{
		PendingChapterQuestId =
			CurrentMainQuestId;
		return;
	}

	StartChapter(CurrentMainQuestId);
}

void UTDInteractionFlowComponent::StartChapter(
	FName QuestId)
{
	FName ChapterId;

	const FTDChapterRow* Row =
		FindChapterForQuest(
			QuestId,
			ChapterId);

	if (Row == nullptr
		|| ChapterId.IsNone())
	{
		SetGameplayLocked(false);
		return;
	}

	PendingChapterQuestId = NAME_None;
	ActiveChapterId = ChapterId;

	FTDChapterPresentationView View;
	View.ChapterId = ChapterId;
	View.ChapterNumber =
		FMath::Max(1, Row->ChapterNumber);
	View.ChapterTitle = Row->ChapterTitle;
	View.BackgroundImage =
		Row->BackgroundImage;
	View.Sound = Row->Sound;
	View.Duration =
		FMath::Max(0.1f, Row->Duration);

	SetGameplayLocked(true);
	ClientShowChapter(View);

	GetWorld()->GetTimerManager().ClearTimer(
		ChapterTimerHandle);

	GetWorld()->GetTimerManager().SetTimer(
		ChapterTimerHandle,
		this,
		&UTDInteractionFlowComponent::
			FinishChapter,
		View.Duration,
		false);
}

void UTDInteractionFlowComponent::FinishChapter()
{
	if (!GetOwner()->HasAuthority()
		|| ActiveChapterId.IsNone())
	{
		return;
	}

	APlayerController* Controller =
		GetOwnerController();

	ATDPlayerState* PlayerState =
		Controller
			? Controller
				->GetPlayerState<ATDPlayerState>()
			: nullptr;

	UTDPersonalWorldStateComponent* Personal =
		PlayerState
			? PlayerState
				->GetPersonalWorldStateComponent()
			: nullptr;

	/**
	 * 연출 종료 순간에만 본 것으로 저장한다.
	 * 연결이 중간에 끊기면 이 함수가 실행되지 않아
	 * 다음 접속에서 다시 나온다.
	 */
	if (Personal != nullptr)
	{
		Personal->MarkChapterSeen(
			ActiveChapterId);
	}

	ActiveChapterId = NAME_None;

	ClientCloseChapter();
	SetGameplayLocked(false);
}

void UTDInteractionFlowComponent::SetGameplayLocked(
	bool bLocked)
{
	bGameplayLocked = bLocked;

	APlayerController* Controller =
		GetOwnerController();

	if (Controller == nullptr)
	{
		return;
	}

	APawn* Pawn = Controller->GetPawn();

	if (ACharacter* Character =
		Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Movement =
			Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();

			if (bLocked)
			{
				Movement->DisableMovement();
			}
			else
			{
				const ATDCharacterBase* TDCharacter =
					Cast<ATDCharacterBase>(Character);

				if (TDCharacter == nullptr
					|| !TDCharacter->IsDead())
				{
					Movement->SetMovementMode(
						MOVE_Walking);
				}
			}
		}
	}

	ClientSetGameplayLocked(bLocked);
}

void UTDInteractionFlowComponent::
ClientSetGameplayLocked_Implementation(
	bool bLocked)
{
	bGameplayLocked = bLocked;

	/**
	 * 대화 또는 챕터 연출로 조작이 잠기면
	 * 이 플레이어 화면의 모든 NPC 퀘스트 마커를 숨긴다.
	 *
	 * 조작 잠금이 해제되면 각 NPC가 현재 퀘스트 상태를
	 * 다시 검사하여 필요한 ! 또는 ?만 복구한다.
	 */
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ATDNPCBase> It(World);
			 It;
			 ++It)
		{
			It->SetQuestMarkerSuppressed(
				bLocked);
		}
	}

	APlayerController* Controller =
		GetOwnerController();

	if (Controller == nullptr)
	{
		return;
	}

	Controller->SetIgnoreMoveInput(bLocked);
	Controller->SetIgnoreLookInput(bLocked);

	if (ACharacter* Character =
		Cast<ACharacter>(
			Controller->GetPawn()))
	{
		if (UCharacterMovementComponent* Movement =
			Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();

			if (bLocked)
			{
				Movement->DisableMovement();
			}
			else
			{
				const ATDCharacterBase* TDCharacter =
					Cast<ATDCharacterBase>(Character);

				if (TDCharacter == nullptr
					|| !TDCharacter->IsDead())
				{
					Movement->SetMovementMode(
						MOVE_Walking);
				}
			}
		}
	}

	if (bLocked)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);

		Controller->SetInputMode(InputMode);
		Controller->bShowMouseCursor = true;
	}
	else
	{
		Controller->SetInputMode(
			FInputModeGameOnly());

		Controller->bShowMouseCursor = false;
	}
}

void UTDInteractionFlowComponent::
BeginDialogueFromSource(
	AActor* Source,
	FName StartRow)
{
	if (!GetOwner()->HasAuthority()
		|| !IsValid(Source)
		|| StartRow.IsNone()
		|| bGameplayLocked
		|| !Source->Implements<
			UTDDialogueSource>())
	{
		return;
	}

	APlayerController* Controller =
		GetOwnerController();

	if (!ITDDialogueSource::
		Execute_IsDialogueSourceInRange(
			Source,
			Controller->GetPawn()))
	{
		return;
	}

	UDataTable* Table =
		ITDDialogueSource::
			Execute_GetDialogueTable(Source);

	if (Table == nullptr
		|| Table->FindRow<FTDDialogueRow>(
			StartRow,
			TEXT("BeginDialogueFromSource"),
			false) == nullptr)
	{
		return;
	}

	++DialogueSessionCounter;

	if (DialogueSessionCounter <= 0)
	{
		DialogueSessionCounter = 1;
	}

	ActiveDialogueSessionId =
		DialogueSessionCounter;

	ActiveDialogueSource = Source;
	ActiveDialogueTable = Table;
	ActiveDialogueRow = StartRow;

	SetGameplayLocked(true);
	SendCurrentDialogueLine();
}

void UTDInteractionFlowComponent::
SendCurrentDialogueLine()
{
	if (!GetOwner()->HasAuthority()
		|| ActiveDialogueSessionId == 0
		|| !IsValid(ActiveDialogueSource)
		|| ActiveDialogueTable == nullptr
		|| ActiveDialogueRow.IsNone())
	{
		EndDialogueSession();
		return;
	}

	const FTDDialogueRow* Row =
		ActiveDialogueTable
			->FindRow<FTDDialogueRow>(
				ActiveDialogueRow,
				TEXT("SendCurrentDialogueLine"),
				false);

	if (Row == nullptr)
	{
		EndDialogueSession();
		return;
	}

	APlayerController* Controller =
		GetOwnerController();

	const ATDPlayerState* PlayerState =
		Controller
			? Controller
				->GetPlayerState<ATDPlayerState>()
			: nullptr;

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
		View.SpeakerName =
			PlayerState != nullptr
				? FText::FromString(
					PlayerState->GetPlayerName())
				: NSLOCTEXT(
					"TDDialogue",
					"Player",
					"플레이어");
	}
	else
	{
		View.SpeakerName =
			ITDDialogueSource::
				Execute_GetDialogueDisplayName(
					ActiveDialogueSource);
	}

	if (!Row->PortraitOverride.IsNull())
	{
		View.Portrait =
			Row->PortraitOverride;
	}
	else if (!Row->bPlayerSpeaker)
	{
		View.Portrait =
			ITDDialogueSource::
				Execute_GetDialoguePortrait(
					ActiveDialogueSource);
	}

	View.ContinueButtonText =
		Row->ContinueButtonText.IsEmpty()
			? NSLOCTEXT(
				"TDDialogue",
				"Continue",
				"다음")
			: Row->ContinueButtonText;

	if (Row->OnAdvanceAction.ActionType ==
		TDTags::Dialogue_Action_AcceptQuest.GetTag())
	{
		View.bShowAcceptDecline = true;
		View.OfferedQuestId =
			Row->OnAdvanceAction.QuestId;

		if (UTDQuestComponent* Quest =
			GetQuestComponent())
		{
			if (const FTDQuestRow* Definition =
				Quest->GetQuestDefinition(
					View.OfferedQuestId))
			{
				View.OfferedQuestName =
					Definition->DisplayName;

				View.OfferedQuestDescription =
					Definition->Description;

				View.OfferedQuestObjectives =
					Definition->Objectives;

				View.OfferedQuestItemRewards =
					Definition->ItemRewards;

				View.OfferedQuestExpReward =
					Definition->ExpReward;

				View.OfferedQuestGoldReward =
					Definition->GoldReward;

				View.OfferedQuestAffectionRewards =
					Definition->AffectionRewards;
			}
		}
	}

	if (const ATDNPCBase* NPC =
		Cast<ATDNPCBase>(
			ActiveDialogueSource))
	{
		View.bShowGiftButton =
			NPC->CanReceiveGifts();

		const UTDPersonalWorldStateComponent* Personal =
			PlayerState
				? PlayerState
					->GetPersonalWorldStateComponent()
				: nullptr;

		View.bGiftAvailableToday =
			Personal != nullptr
			&& Personal->CanGiftToNPC(
				NPC->GetNPCId());
	}

	ClientShowDialogueLine(
		ActiveDialogueSessionId,
		ActiveDialogueRow,
		View);
}

bool UTDInteractionFlowComponent::
ValidateDialogueRequest(
	int32 SessionId,
	FName ExpectedCurrentRow) const
{
	if (SessionId != ActiveDialogueSessionId
		|| ExpectedCurrentRow != ActiveDialogueRow
		|| !IsValid(ActiveDialogueSource)
		|| ActiveDialogueTable == nullptr)
	{
		return false;
	}

	const APlayerController* Controller =
		GetOwnerController();

	return Controller != nullptr
		&& ITDDialogueSource::
			Execute_IsDialogueSourceInRange(
				ActiveDialogueSource,
				Controller->GetPawn());
}

bool UTDInteractionFlowComponent::
ApplyDialogueAction(
	const FTDDialogueAction& Action)
{
	if (!Action.ActionType.IsValid())
	{
		return true;
	}

	UTDQuestComponent* Quest =
		GetQuestComponent();

	if (Quest == nullptr)
	{
		ClientFlowQuestActionResult(
			Action.QuestId,
			ETDQuestActionResult::
				InvalidDefinition);

		return false;
	}

	const ETDQuestTargetType SourceType =
		ITDDialogueSource::
			Execute_GetDialogueQuestTargetType(
				ActiveDialogueSource);

	const FName SourceId =
		ITDDialogueSource::
			Execute_GetDialogueSourceId(
				ActiveDialogueSource);

	ETDQuestActionResult Result =
		ETDQuestActionResult::Success;

	if (Action.ActionType ==
		TDTags::Dialogue_Action_AcceptQuest.GetTag())
	{
		Result = Quest->AcceptQuestAtTarget(
			Action.QuestId,
			SourceType,
			SourceId);
	}
	else if (Action.ActionType ==
		TDTags::Dialogue_Action_TurnInQuest.GetTag())
	{
		Result = Quest->TurnInQuestAtTarget(
			Action.QuestId,
			SourceType,
			SourceId);
	}
	else if (Action.ActionType ==
		TDTags::
			Dialogue_Action_CompleteDialogueQuest
				.GetTag())
	{
		/**
		 * 대화 자체가 목표인 퀘스트 전용 처리.
		 *
		 * 첫 번째로 대화 이벤트 목표를 달성시키고,
		 * 같은 서버 입력 안에서 즉시 완료와 보상 지급까지 처리한다.
		 *
		 * ReadyToTurnIn 상태가 한 프레임 동안 유지되지 않기 때문에
		 * NPC 머리 위 마커가 대화 중간에 ?로 바뀌어 보이지 않는다.
		 */
		Quest->ReportQuestEvent(
			Action.EventTag,
			FMath::Max(
				1,
				Action.EventAmount));

		Result = Quest->TurnInQuestAtTarget(
			Action.QuestId,
			SourceType,
			SourceId);
	}
	else if (Action.ActionType ==
		TDTags::
			Dialogue_Action_ReportQuestEvent.GetTag())
	{
		Quest->ReportQuestEvent(
			Action.EventTag,
			FMath::Max(
				1,
				Action.EventAmount));

		return true;
	}
	else
	{
		Result =
			ETDQuestActionResult::
				InvalidDefinition;
	}

	ClientFlowQuestActionResult(
		Action.QuestId,
		Result);

	return Result ==
		ETDQuestActionResult::Success;
}

void UTDInteractionFlowComponent::
ServerAdvanceDialogue_Implementation(
	int32 SessionId,
	FName ExpectedCurrentRow)
{
	if (!ValidateDialogueRequest(
		SessionId,
		ExpectedCurrentRow))
	{
		EndDialogueSession();
		return;
	}

	const FTDDialogueRow* Row =
		ActiveDialogueTable
			->FindRow<FTDDialogueRow>(
				ActiveDialogueRow,
				TEXT("ServerAdvanceDialogue"),
				false);

	if (Row == nullptr)
	{
		EndDialogueSession();
		return;
	}

	/**
	 * 수락 행은 일반 다음 버튼으로 처리하지 않는다.
	 * 반드시 수락 또는 거절 버튼을 사용한다.
	 */
	if (Row->OnAdvanceAction.ActionType ==
		TDTags::Dialogue_Action_AcceptQuest.GetTag())
	{
		return;
	}

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

void UTDInteractionFlowComponent::
ServerAcceptQuest_Implementation(
	int32 SessionId,
	FName ExpectedCurrentRow)
{
	if (!ValidateDialogueRequest(
		SessionId,
		ExpectedCurrentRow))
	{
		EndDialogueSession();
		return;
	}

	const FTDDialogueRow* Row =
		ActiveDialogueTable
			->FindRow<FTDDialogueRow>(
				ActiveDialogueRow,
				TEXT("ServerAcceptQuest"),
				false);

	if (Row == nullptr
		|| Row->OnAdvanceAction.ActionType !=
			TDTags::Dialogue_Action_AcceptQuest.GetTag())
	{
		EndDialogueSession();
		return;
	}

	ApplyDialogueAction(Row->OnAdvanceAction);

	/**
	 * 성공·실패와 관계없이 수락 대화는 닫는다.
	 * 실패한 경우 다시 F를 누르면 첫 줄부터 시작한다.
	 */
	EndDialogueSession();
}

void UTDInteractionFlowComponent::
ServerDeclineQuest_Implementation(
	int32 SessionId)
{
	if (SessionId == ActiveDialogueSessionId)
	{
		EndDialogueSession();
	}
}

void UTDInteractionFlowComponent::
ServerCancelDialogue_Implementation(
	int32 SessionId)
{
	if (SessionId == ActiveDialogueSessionId)
	{
		EndDialogueSession();
	}
}

void UTDInteractionFlowComponent::
ServerGiveGift_Implementation(
	int32 SessionId,
	int32 InventorySlot)
{
	FTDGiftResultView Result;

	if (SessionId != ActiveDialogueSessionId)
	{
		ClientGiftResult(Result);
		return;
	}

	ATDNPCBase* NPC =
		Cast<ATDNPCBase>(
			ActiveDialogueSource);

	APlayerController* Controller =
		GetOwnerController();

	ATDPlayerState* PlayerState =
		Controller
			? Controller
				->GetPlayerState<ATDPlayerState>()
			: nullptr;

	UTDInventoryComponent* Inventory =
		PlayerState
			? PlayerState
				->GetInventoryComponent()
			: nullptr;

	UTDQuestComponent* Quest =
		PlayerState
			? PlayerState->GetQuestComponent()
			: nullptr;

	UTDPersonalWorldStateComponent* Personal =
		PlayerState
			? PlayerState
				->GetPersonalWorldStateComponent()
			: nullptr;

	if (NPC == nullptr
		|| !NPC->CanReceiveGifts()
		|| Inventory == nullptr
		|| Quest == nullptr
		|| Personal == nullptr)
	{
		Result.Result =
			ETDGiftActionResult::InvalidNPC;

		ClientGiftResult(Result);
		return;
	}

	Result.NPCId = NPC->GetNPCId();

	if (!Personal->CanGiftToNPC(Result.NPCId))
	{
		Result.Result =
			ETDGiftActionResult::
				AlreadyGiftedToday;

		ClientGiftResult(Result);
		return;
	}

	const FTDItemInstance* Item =
		Inventory->FindBySlot(
			InventorySlot);

	if (Item == nullptr || Item->Count <= 0)
	{
		Result.Result =
			ETDGiftActionResult::InvalidItem;

		ClientGiftResult(Result);
		return;
	}

	const FName ItemId = Item->ItemId;

	const FTDItemRow* Definition =
		Inventory->FindItemDefinition(ItemId);

	if (Definition == nullptr)
	{
		Result.Result =
			ETDGiftActionResult::InvalidItem;

		ClientGiftResult(Result);
		return;
	}

	if (Definition->ItemType ==
		TDTags::Item_Type_Quest.GetTag())
	{
		Result.Result =
			ETDGiftActionResult::
				QuestItemNotAllowed;

		ClientGiftResult(Result);
		return;
	}

	const int32 AffectionGain =
		NPC->GetGiftAffectionValue(ItemId);

	/**
	 * 선물은 종류와 관계없이 정확히 1개만 제거한다.
	 */
	if (!Inventory->ConsumeItemAt(
		InventorySlot,
		1))
	{
		Result.Result =
			ETDGiftActionResult::ConsumeFailed;

		ClientGiftResult(Result);
		return;
	}

	Quest->AddAffection(
		Result.NPCId,
		AffectionGain);

	if (!Personal->MarkGiftGiven(
		Result.NPCId))
	{
		UE_LOG(LogTemp, Error,
			TEXT("선물 소비 후 일일 기록 실패: NPC='%s'"),
			*Result.NPCId.ToString());
	}

	Result.Result =
		ETDGiftActionResult::Success;

	Result.NPCResponse =
		NPC->GetGiftThankYouText();

	Result.AffectionGained =
		AffectionGain;

	Result.TotalAffection =
		Quest->GetAffectionPoints(
			Result.NPCId);

	ClientGiftResult(Result);

	/**
	 * 같은 대화창에서 선물 버튼을 즉시 비활성화한다.
	 */
	SendCurrentDialogueLine();
}

TArray<FTDGiftItemView>
UTDInteractionFlowComponent::
GetGiftItemViews() const
{
	TArray<FTDGiftItemView> Result;

	const APlayerController* Controller =
		GetOwnerController();

	const ATDPlayerState* PlayerState =
		Controller
			? Controller
				->GetPlayerState<ATDPlayerState>()
			: nullptr;

	const UTDInventoryComponent* Inventory =
		PlayerState
			? PlayerState
				->GetInventoryComponent()
			: nullptr;

	const UTDItemUseComponent* ItemUse =
		PlayerState
			? PlayerState->GetItemUseComponent()
			: nullptr;

	if (Inventory == nullptr)
	{
		return Result;
	}

	for (const FTDItemInstance& Item :
		Inventory->GetItems())
	{
		FTDGiftItemView& View =
			Result.AddDefaulted_GetRef();

		View.ItemId = Item.ItemId;
		View.SlotIndex = Item.SlotIndex;
		View.Count = Item.Count;
		View.Location =
			ETDGiftItemLocation::Inventory;

		const FTDItemRow* Definition =
			Inventory->FindItemDefinition(
				Item.ItemId);

		if (Definition != nullptr)
		{
			View.DisplayName =
				Definition->DisplayName;

			View.Icon = Definition->Icon;

			View.bCanGift =
				Definition->ItemType !=
					TDTags::Item_Type_Quest.GetTag();
		}
	}

	if (ItemUse != nullptr)
	{
		for (const FTDItemInstance& Item :
			ItemUse->GetEquippedItems())
		{
			FTDGiftItemView& View =
				Result.AddDefaulted_GetRef();

			View.ItemId = Item.ItemId;
			View.SlotIndex = Item.SlotIndex;
			View.Count = Item.Count;
			View.Location =
				ETDGiftItemLocation::Equipped;
			View.bCanGift = false;

			const FTDItemRow* Definition =
				Inventory->FindItemDefinition(
					Item.ItemId);

			if (Definition != nullptr)
			{
				View.DisplayName =
					Definition->DisplayName;

				View.Icon = Definition->Icon;
			}
		}
	}

	return Result;
}

void UTDInteractionFlowComponent::
EndDialogueSession()
{
	const int32 EndedSessionId =
		ActiveDialogueSessionId;

	ActiveDialogueSessionId = 0;
	ActiveDialogueSource = nullptr;
	ActiveDialogueTable = nullptr;
	ActiveDialogueRow = NAME_None;

	if (EndedSessionId != 0)
	{
		ClientCloseDialogue(
			EndedSessionId);
	}

	if (!PendingChapterQuestId.IsNone())
	{
		const FName ChapterQuestId =
			PendingChapterQuestId;

		PendingChapterQuestId = NAME_None;
		StartChapter(ChapterQuestId);
		return;
	}

	SetGameplayLocked(false);
}

void UTDInteractionFlowComponent::
ClientShowDialogueLine_Implementation(
	int32 SessionId,
	FName DialogueRow,
	FTDDialogueLineView Line)
{
	OnDialogueLine.Broadcast(
		SessionId,
		DialogueRow,
		Line);
}

void UTDInteractionFlowComponent::
ClientCloseDialogue_Implementation(
	int32 SessionId)
{
	OnDialogueClosed.Broadcast(SessionId);
}

void UTDInteractionFlowComponent::
ClientFlowQuestActionResult_Implementation(
	FName QuestId,
	ETDQuestActionResult Result)
{
	OnQuestActionResult.Broadcast(
		QuestId,
		Result);
}

void UTDInteractionFlowComponent::
ClientGiftResult_Implementation(
	FTDGiftResultView Result)
{
	OnGiftResult.Broadcast(Result);
}

void UTDInteractionFlowComponent::
ClientShowChapter_Implementation(
	FTDChapterPresentationView Chapter)
{
	OnChapterShown.Broadcast(Chapter);
}

void UTDInteractionFlowComponent::
ClientCloseChapter_Implementation()
{
	OnChapterClosed.Broadcast();
}