#include "Interaction/TDInteractionFlowComponent.h"

#include "Character/TDCharacterBase.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDItemRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Interaction/TDDialogueSource.h"
#include "Items/TDInventoryComponent.h"
#include "Items/TDItemUseComponent.h"
#include "Player/TDPlayerState.h"
#include "Quest/TDPersonalWorldStateComponent.h"
#include "Quest/TDQuestComponent.h"
#include "TimerManager.h"
#include "Widgets/SViewport.h"
#include "World/TDNPCBase.h"
#include "Enhance/TDEnhanceServiceComponent.h"
#include "Shop/TDShopServiceComponent.h"

UTDInteractionFlowComponent::UTDInteractionFlowComponent()
{
	// 로컬 대화 중 ESC 확인에만 Tick을 사용한다.
	// 이동, 공격, 스킬 입력은 처리하지 않는다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

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
	const APlayerController* OwningController =
		GetOwnerController();

	const ATDPlayerState* OwningPlayerState =
		OwningController
			? OwningController->GetPlayerState<ATDPlayerState>()
			: nullptr;

	return OwningPlayerState
		? OwningPlayerState->GetQuestComponent()
		: nullptr;
}

void UTDInteractionFlowComponent::BeginPlay()
{
	Super::BeginPlay();

	TryBindQuestComponent();

	if (BoundQuestComponent == nullptr && GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().SetTimer(
			BindRetryTimerHandle,
			this,
			&UTDInteractionFlowComponent::TryBindQuestComponent,
			0.25f,
			true);
	}
}

void UTDInteractionFlowComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	SetComponentTickEnabled(false);

	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(
			BindRetryTimerHandle);

		GetWorld()->GetTimerManager().ClearTimer(
			ChapterTimerHandle);

		GetWorld()->GetTimerManager().ClearTimer(
			DialogueDistanceTimerHandle);
	}

	if (IsValid(BoundQuestComponent))
	{
		BoundQuestComponent->OnQuestListChanged.RemoveDynamic(
			this,
			&UTDInteractionFlowComponent::HandleQuestListChanged);
	}

	// 종료 중에는 다음 챕터를 시작하지 않고 로컬 표시만 정리한다.
	LocalDialogueSessionId = 0;
	bLocalChapterVisible = false;
	RefreshLocalPresentation();

	ActiveDialogueSessionId = 0;
	ActiveDialogueSource = nullptr;
	ActiveDialogueTable = nullptr;
	ActiveDialoguePawn = nullptr;
	ActiveDialogueRow = NAME_None;

	Super::EndPlay(EndPlayReason);
}

void UTDInteractionFlowComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APlayerController* OwningController = GetOwnerController();

	if (!IsValid(OwningController)
		|| !OwningController->IsLocalController()
		|| LocalDialogueSessionId == 0)
	{
		return;
	}

	// ESC만 관찰한다.
	// 이동, 공격, 스킬, 아이템 입력에는 개입하지 않는다.
	const bool bEscapeDown =
		OwningController->IsInputKeyDown(EKeys::Escape);

	const bool bEscapePressed =
		bEscapeDown && !bEscapeWasDown;

	bEscapeWasDown = bEscapeDown;

	if (bEscapePressed)
	{
		ServerCancelDialogue(LocalDialogueSessionId);
	}
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

	BoundQuestComponent->OnQuestListChanged.AddUniqueDynamic(
		this,
		&UTDInteractionFlowComponent::HandleQuestListChanged);

	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(
			BindRetryTimerHandle);
	}

	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		TryStartCurrentChapter();
	}
}

void UTDInteractionFlowComponent::HandleQuestListChanged()
{
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
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

	if (ChapterTable == nullptr || QuestId.IsNone())
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
			if (Found == nullptr && Row.StartQuestId == QuestId)
			{
				Found = &Row;
				OutChapterId = RowName;
			}
		});

	return Found;
}

void UTDInteractionFlowComponent::TryStartCurrentChapter()
{
	if (GetOwner() == nullptr
		|| !GetOwner()->HasAuthority()
		|| !ActiveChapterId.IsNone()
		|| BoundQuestComponent == nullptr)
	{
		return;
	}

	FName CurrentMainQuestId = NAME_None;

	for (const FTDQuestViewData& View :
		BoundQuestComponent->GetQuestTrackerViews())
	{
		if (View.QuestTypeTag == TDTags::Quest_Type_Main.GetTag())
		{
			CurrentMainQuestId = View.QuestId;
			break;
		}
	}

	if (CurrentMainQuestId.IsNone())
	{
		return;
	}

	FName ChapterId = NAME_None;

	const FTDChapterRow* Chapter =
		FindChapterForQuest(CurrentMainQuestId, ChapterId);

	if (Chapter == nullptr || ChapterId.IsNone())
	{
		return;
	}

	const APlayerController* OwningController =
		GetOwnerController();

	const ATDPlayerState* OwningPlayerState =
		OwningController
			? OwningController->GetPlayerState<ATDPlayerState>()
			: nullptr;

	const UTDPersonalWorldStateComponent* Personal =
		OwningPlayerState
			? OwningPlayerState->GetPersonalWorldStateComponent()
			: nullptr;

	if (Personal == nullptr || Personal->HasSeenChapter(ChapterId))
	{
		return;
	}

	// 대화 완료로 새 챕터가 시작되면 대화창이 닫힌 뒤 표시한다.
	if (ActiveDialogueSessionId != 0)
	{
		PendingChapterQuestId = CurrentMainQuestId;
		return;
	}

	StartChapter(CurrentMainQuestId);
}

void UTDInteractionFlowComponent::StartChapter(FName QuestId)
{
	if (GetOwner() == nullptr
		|| !GetOwner()->HasAuthority()
		|| GetWorld() == nullptr
		|| !ActiveChapterId.IsNone())
	{
		return;
	}

	FName ChapterId = NAME_None;

	const FTDChapterRow* Row =
		FindChapterForQuest(QuestId, ChapterId);

	if (Row == nullptr || ChapterId.IsNone())
	{
		return;
	}

	PendingChapterQuestId = NAME_None;
	ActiveChapterId = ChapterId;

	FTDChapterPresentationView View;
	View.ChapterId = ChapterId;
	View.ChapterNumber = FMath::Max(1, Row->ChapterNumber);
	View.ChapterTitle = Row->ChapterTitle;
	View.BackgroundImage = Row->BackgroundImage;
	View.Sound = Row->Sound;
	View.Duration = FMath::Max(0.1f, Row->Duration);

	// 화면만 표시한다. 이동 잠금과 무적을 적용하지 않는다.
	ClientShowChapter(View);

	GetWorld()->GetTimerManager().ClearTimer(
		ChapterTimerHandle);

	GetWorld()->GetTimerManager().SetTimer(
		ChapterTimerHandle,
		this,
		&UTDInteractionFlowComponent::FinishChapter,
		View.Duration,
		false);
}

void UTDInteractionFlowComponent::FinishChapter()
{
	if (GetOwner() == nullptr
		|| !GetOwner()->HasAuthority()
		|| ActiveChapterId.IsNone())
	{
		return;
	}

	APlayerController* OwningController =
		GetOwnerController();

	ATDPlayerState* OwningPlayerState =
		OwningController
			? OwningController->GetPlayerState<ATDPlayerState>()
			: nullptr;

	UTDPersonalWorldStateComponent* Personal =
		OwningPlayerState
			? OwningPlayerState->GetPersonalWorldStateComponent()
			: nullptr;

	// 기존 규칙 유지: 연출 종료 시점에 본 것으로 기록한다.
	if (Personal != nullptr)
	{
		Personal->MarkChapterSeen(ActiveChapterId);
	}

	ActiveChapterId = NAME_None;

	ClientCloseChapter();

	// 연출 중 메인 퀘스트가 변경된 경우도 확인한다.
	TryStartCurrentChapter();
}

bool UTDInteractionFlowComponent::IsDialogueContextValid() const
{
	if (ActiveDialogueSessionId == 0
		|| !IsValid(ActiveDialogueSource)
		|| !IsValid(ActiveDialogueTable)
		|| !IsValid(ActiveDialoguePawn)
		|| ActiveDialogueRow.IsNone()
		|| !ActiveDialogueSource->Implements<UTDDialogueSource>())
	{
		return false;
	}

	const APlayerController* OwningController =
		GetOwnerController();

	if (!IsValid(OwningController)
		|| OwningController->GetPawn() != ActiveDialoguePawn.Get())
	{
		return false;
	}

	const ATDCharacterBase* DialogueCharacter =
		Cast<ATDCharacterBase>(ActiveDialoguePawn.Get());

	if (DialogueCharacter != nullptr && DialogueCharacter->IsDead())
	{
		return false;
	}

	if (ActiveDialogueSource->GetWorld()
		!= ActiveDialoguePawn->GetWorld())
	{
		return false;
	}

	// NPC는 DialogueContinueDistance,
	// 퀘스트 물건은 데이터의 DialogueDistance를 사용한다.
	return ITDDialogueSource::Execute_IsDialogueSourceInRange(
		ActiveDialogueSource.Get(),
		ActiveDialoguePawn.Get());
}

void UTDInteractionFlowComponent::CheckDialogueDistance()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (ActiveDialogueSessionId == 0)
	{
		if (GetWorld() != nullptr)
		{
			GetWorld()->GetTimerManager().ClearTimer(
				DialogueDistanceTimerHandle);
		}
		return;
	}

	if (!IsDialogueContextValid())
	{
		EndDialogueSession();
	}
}

void UTDInteractionFlowComponent::BeginDialogueFromSource(
	AActor* Source,
	FName StartRow)
{
	if (GetOwner() == nullptr
		|| !GetOwner()->HasAuthority()
		|| GetWorld() == nullptr
		|| !IsValid(Source)
		|| StartRow.IsNone()
		|| !Source->Implements<UTDDialogueSource>())
	{
		return;
	}

	// 기존 대화가 유효하면 F 연타로 새 대화를 덮어쓰지 않는다.
	if (ActiveDialogueSessionId != 0)
	{
		CheckDialogueDistance();

		if (ActiveDialogueSessionId != 0)
		{
			return;
		}
	}

	APlayerController* OwningController =
		GetOwnerController();

	APawn* DialoguePawn =
		OwningController ? OwningController->GetPawn() : nullptr;

	if (!IsValid(DialoguePawn))
	{
		return;
	}

	const ATDCharacterBase* DialogueCharacter =
		Cast<ATDCharacterBase>(DialoguePawn);

	if (DialogueCharacter != nullptr && DialogueCharacter->IsDead())
	{
		return;
	}

	if (!ITDDialogueSource::Execute_IsDialogueSourceInRange(
		Source,
		DialoguePawn))
	{
		return;
	}

	UDataTable* Table =
		ITDDialogueSource::Execute_GetDialogueTable(Source);

	if (Table == nullptr
		|| Table->FindRow<FTDDialogueRow>(
			StartRow,
			TEXT("BeginDialogueFromSource"),
			false) == nullptr)
	{
		return;
	}
	
	// 새 대화가 시작되면 열려 있던 강화창과 강화 이용 상태를 종료합니다.
	if (UTDEnhanceServiceComponent* Enhance =
		GetOwner()->FindComponentByClass<UTDEnhanceServiceComponent>())
	{
		Enhance->EndService();
	}

	// 상점 이용 중 다시 말을 걸면 기존 상점 세션부터 닫습니다.
	if (UTDShopServiceComponent* Shop =
		GetOwner()->FindComponentByClass<UTDShopServiceComponent>())
	{
		Shop->EndService();
	}
	
	DialogueSessionCounter =
		DialogueSessionCounter >= MAX_int32
			? 1
			: DialogueSessionCounter + 1;

	ActiveDialogueSessionId = DialogueSessionCounter;
	ActiveDialogueSource = Source;
	ActiveDialogueTable = Table;
	ActiveDialoguePawn = DialoguePawn;
	ActiveDialogueRow = StartRow;

	// 캐릭터의 이동, 회전, 스킬 상태는 건드리지 않는다.
	SendCurrentDialogueLine();

	if (ActiveDialogueSessionId != 0)
	{
		GetWorld()->GetTimerManager().SetTimer(
			DialogueDistanceTimerHandle,
			this,
			&UTDInteractionFlowComponent::CheckDialogueDistance,
			0.1f,
			true);
	}
}

void UTDInteractionFlowComponent::SendCurrentDialogueLine()
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!IsDialogueContextValid())
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
		EndDialogueSession();
		return;
	}

	const APlayerController* OwningController =
		GetOwnerController();

	const ATDPlayerState* OwningPlayerState =
		OwningController
			? OwningController->GetPlayerState<ATDPlayerState>()
			: nullptr;

	FTDDialogueLineView View;
	View.bPlayerSpeaker = Row->bPlayerSpeaker;
	View.DialogueText = Row->DialogueText;

	if (!Row->SpeakerNameOverride.IsEmpty())
	{
		View.SpeakerName = Row->SpeakerNameOverride;
	}
	else if (Row->bPlayerSpeaker)
	{
		View.SpeakerName =
			OwningPlayerState
				? FText::FromString(OwningPlayerState->GetPlayerName())
				: NSLOCTEXT("TDDialogue", "Player", "플레이어");
	}
	else
	{
		View.SpeakerName =
			ITDDialogueSource::Execute_GetDialogueDisplayName(
				ActiveDialogueSource.Get());
	}

	if (!Row->PortraitOverride.IsNull())
	{
		View.Portrait = Row->PortraitOverride;
	}
	else if (!Row->bPlayerSpeaker)
	{
		View.Portrait =
			ITDDialogueSource::Execute_GetDialoguePortrait(
				ActiveDialogueSource.Get());
	}

	View.ContinueButtonText =
		Row->ContinueButtonText.IsEmpty()
			? NSLOCTEXT("TDDialogue", "Continue", "다음")
			: Row->ContinueButtonText;

	if (Row->OnAdvanceAction.ActionType
		== TDTags::Dialogue_Action_AcceptQuest.GetTag())
	{
		View.bShowAcceptDecline = true;
		View.OfferedQuestId = Row->OnAdvanceAction.QuestId;

		if (UTDQuestComponent* Quest = GetQuestComponent())
		{
			if (const FTDQuestRow* Definition =
				Quest->GetQuestDefinition(View.OfferedQuestId))
			{
				View.OfferedQuestName = Definition->DisplayName;
				View.OfferedQuestDescription = Definition->Description;
				View.OfferedQuestObjectives = Definition->Objectives;
				View.OfferedQuestItemRewards = Definition->ItemRewards;
				View.OfferedQuestExpReward = Definition->ExpReward;
				View.OfferedQuestGoldReward = Definition->GoldReward;
				View.OfferedQuestAffectionRewards =
					Definition->AffectionRewards;
			}
		}
	}

	if (const ATDNPCBase* NPC =
		Cast<ATDNPCBase>(ActiveDialogueSource.Get()))
	{
		View.bShowGiftButton = NPC->CanReceiveGifts();

		const UTDPersonalWorldStateComponent* Personal =
			OwningPlayerState
				? OwningPlayerState->GetPersonalWorldStateComponent()
				: nullptr;

		View.bGiftAvailableToday =
			Personal != nullptr
			&& Personal->CanGiftToNPC(NPC->GetNPCId());
	}

	ClientShowDialogueLine(
		ActiveDialogueSessionId,
		ActiveDialogueRow,
		View);
}

bool UTDInteractionFlowComponent::ValidateDialogueRequest(
	int32 SessionId,
	FName ExpectedCurrentRow) const
{
	return SessionId > 0
		&& SessionId == ActiveDialogueSessionId
		&& ExpectedCurrentRow == ActiveDialogueRow
		&& IsDialogueContextValid();
}

bool UTDInteractionFlowComponent::ApplyDialogueAction(
	const FTDDialogueAction& Action)
{
	if (!Action.ActionType.IsValid())
	{
		return true;
	}

	UTDQuestComponent* Quest = GetQuestComponent();

	if (Quest == nullptr
		|| !IsValid(ActiveDialogueSource)
		|| Action.QuestId.IsNone())
	{
		ClientFlowQuestActionResult(
			Action.QuestId,
			ETDQuestActionResult::InvalidDefinition);

		return false;
	}

	const ETDQuestTargetType SourceType =
		ITDDialogueSource::Execute_GetDialogueQuestTargetType(
			ActiveDialogueSource.Get());

	const FName SourceId =
		ITDDialogueSource::Execute_GetDialogueSourceId(
			ActiveDialogueSource.Get());

	ETDQuestActionResult Result =
		ETDQuestActionResult::InvalidDefinition;

	if (Action.ActionType
		== TDTags::Dialogue_Action_AcceptQuest.GetTag())
	{
		Result = Quest->AcceptQuestAtTarget(
			Action.QuestId,
			SourceType,
			SourceId);
	}
	else if (Action.ActionType
		== TDTags::Dialogue_Action_TurnInQuest.GetTag())
	{
		Result = Quest->TurnInQuestAtTarget(
			Action.QuestId,
			SourceType,
			SourceId);
	}
	else if (Action.ActionType
		== TDTags::Dialogue_Action_CompleteDialogueQuest.GetTag())
	{
		const FTDQuestRow* Definition =
			Quest->GetQuestDefinition(Action.QuestId);

		if (Definition == nullptr
			|| Definition->bAutoCompleteWithoutTurnIn)
		{
			Result = ETDQuestActionResult::InvalidDefinition;
		}
		else if (Definition->TurnInTargetType != SourceType
			|| Definition->TurnInTargetId != SourceId)
		{
			Result = ETDQuestActionResult::WrongTurnInTarget;
		}
		else
		{
			// 현재 대화가 지정한 퀘스트만 진행시킨다.
			Result = Quest->ReportQuestEventForQuest(
				Action.QuestId,
				Action.EventTag,
				FMath::Max(1, Action.EventAmount));

			if (Result == ETDQuestActionResult::Success)
			{
				Result = Quest->TurnInQuestAtTarget(
					Action.QuestId,
					SourceType,
					SourceId);
			}
		}
	}
	else if (Action.ActionType
		== TDTags::Dialogue_Action_ReportQuestEvent.GetTag())
	{
		Result = Quest->ReportQuestEventForQuest(
			Action.QuestId,
			Action.EventTag,
			FMath::Max(1, Action.EventAmount));

		if (Result == ETDQuestActionResult::Success)
		{
			return true;
		}
	}

	if (Result != ETDQuestActionResult::Success)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("대화 액션 실패: Row='%s', Quest='%s', Action='%s', Result=%d"),
			*ActiveDialogueRow.ToString(),
			*Action.QuestId.ToString(),
			*Action.ActionType.ToString(),
			static_cast<int32>(Result));
	}

	ClientFlowQuestActionResult(Action.QuestId, Result);

	return Result == ETDQuestActionResult::Success;
}

void UTDInteractionFlowComponent::ServerAdvanceDialogue_Implementation(
	int32 SessionId,
	FName ExpectedCurrentRow)
{
	// 이전 대화에서 늦게 도착한 요청은 현재 대화를 건드리지 않는다.
	if (SessionId <= 0 || SessionId != ActiveDialogueSessionId)
	{
		return;
	}

	if (!ValidateDialogueRequest(SessionId, ExpectedCurrentRow))
	{
		if (!IsDialogueContextValid())
		{
			EndDialogueSession();
		}
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

	// 수락 행은 전용 수락/거절 버튼으로 처리한다.
	if (Row->OnAdvanceAction.ActionType
		== TDTags::Dialogue_Action_AcceptQuest.GetTag())
	{
		return;
	}

	if (!ApplyDialogueAction(Row->OnAdvanceAction))
	{
		EndDialogueSession();
		return;
	}

	if (Row->NextRow.IsNone())
	{
		EndDialogueSession(true);
		return;
	}

	ActiveDialogueRow = Row->NextRow;
	SendCurrentDialogueLine();
}

void UTDInteractionFlowComponent::ServerAcceptQuest_Implementation(
	int32 SessionId,
	FName ExpectedCurrentRow)
{
	// 이전 대화에서 늦게 도착한 요청은 무시한다.
	if (SessionId <= 0 || SessionId != ActiveDialogueSessionId)
	{
		return;
	}

	// 현재 대화, 표시된 행, 대화 대상, 거리 등을 검증한다.
	if (!ValidateDialogueRequest(SessionId, ExpectedCurrentRow))
	{
		if (!IsDialogueContextValid())
		{
			EndDialogueSession();
		}

		return;
	}

	const FTDDialogueRow* Row =
		ActiveDialogueTable->FindRow<FTDDialogueRow>(
			ActiveDialogueRow,
			TEXT("ServerAcceptQuest"),
			false);

	// 실제 퀘스트 수락 행에서만 수락을 처리한다.
	if (Row == nullptr
		|| Row->OnAdvanceAction.ActionType
			!= TDTags::Dialogue_Action_AcceptQuest.GetTag())
	{
		EndDialogueSession();
		return;
	}

	const FName QuestId = Row->OnAdvanceAction.QuestId;

	ETDQuestActionResult Result =
		ETDQuestActionResult::InvalidDefinition;

	UTDQuestComponent* Quest = GetQuestComponent();

	if (Quest != nullptr && !QuestId.IsNone())
	{
		const ETDQuestTargetType SourceType =
			ITDDialogueSource::Execute_GetDialogueQuestTargetType(
				ActiveDialogueSource.Get());

		const FName SourceId =
			ITDDialogueSource::Execute_GetDialogueSourceId(
				ActiveDialogueSource.Get());

		// 기존 서버 수락 함수를 그대로 사용한다.
		// 서브 2개 제한, 수락 대상, 선행 조건 등의 검사를 유지한다.
		Result = Quest->AcceptQuestAtTarget(
			QuestId,
			SourceType,
			SourceId);
	}

	if (Result != ETDQuestActionResult::Success)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("퀘스트 수락 실패: Row='%s', Quest='%s', Result=%d"),
			*ActiveDialogueRow.ToString(),
			*QuestId.ToString(),
			static_cast<int32>(Result));
	}

	// 요청한 플레이어의 대화 위젯으로 결과를 전달한다.
	ClientFlowQuestActionResult(QuestId, Result);

	if (Result == ETDQuestActionResult::ActiveSubQuestLimit)
	{
		// 서브 퀘스트 슬롯 부족일 때만 수락 화면을 유지한다.
		// 위젯이 경고 문구와 경고음을 처리한다.
		return;
	}

	// 수락 성공일 때만 강화창을 열 수 있는 정상 종료로 처리합니다.
	EndDialogueSession(Result == ETDQuestActionResult::Success);
}

void UTDInteractionFlowComponent::ServerDeclineQuest_Implementation(
	int32 SessionId)
{
	if (SessionId > 0 && SessionId == ActiveDialogueSessionId)
	{
		EndDialogueSession();
	}
}

void UTDInteractionFlowComponent::ServerCancelDialogue_Implementation(
	int32 SessionId)
{
	if (SessionId > 0 && SessionId == ActiveDialogueSessionId)
	{
		EndDialogueSession();
	}
}

void UTDInteractionFlowComponent::ServerGiveGift_Implementation(
	int32 SessionId,
	int32 InventorySlot)
{
	if (SessionId <= 0 || SessionId != ActiveDialogueSessionId)
	{
		return;
	}

	FTDGiftResultView Result;

	// 자동 종료 타이머 사이에 들어온 요청도 거리부터 검사한다.
	if (!IsDialogueContextValid())
	{
		Result.Result = ETDGiftActionResult::InvalidNPC;
		ClientGiftResult(Result);
		EndDialogueSession();
		return;
	}

	ATDNPCBase* NPC =
		Cast<ATDNPCBase>(ActiveDialogueSource.Get());

	APlayerController* OwningController =
		GetOwnerController();

	ATDPlayerState* OwningPlayerState =
		OwningController
			? OwningController->GetPlayerState<ATDPlayerState>()
			: nullptr;

	UTDInventoryComponent* Inventory =
		OwningPlayerState
			? OwningPlayerState->GetInventoryComponent()
			: nullptr;

	UTDQuestComponent* Quest =
		OwningPlayerState
			? OwningPlayerState->GetQuestComponent()
			: nullptr;

	UTDPersonalWorldStateComponent* Personal =
		OwningPlayerState
			? OwningPlayerState->GetPersonalWorldStateComponent()
			: nullptr;

	if (NPC == nullptr
		|| !NPC->CanReceiveGifts()
		|| Inventory == nullptr
		|| Quest == nullptr
		|| Personal == nullptr)
	{
		Result.Result = ETDGiftActionResult::InvalidNPC;
		ClientGiftResult(Result);
		return;
	}

	Result.NPCId = NPC->GetNPCId();

	if (!Personal->CanGiftToNPC(Result.NPCId))
	{
		Result.Result = ETDGiftActionResult::AlreadyGiftedToday;
		ClientGiftResult(Result);
		return;
	}

	const FTDItemInstance* Item =
		Inventory->FindBySlot(InventorySlot);

	if (Item == nullptr || Item->Count <= 0)
	{
		Result.Result = ETDGiftActionResult::InvalidItem;
		ClientGiftResult(Result);
		return;
	}

	const FName ItemId = Item->ItemId;

	const FTDItemRow* Definition =
		Inventory->FindItemDefinition(ItemId);

	if (Definition == nullptr)
	{
		Result.Result = ETDGiftActionResult::InvalidItem;
		ClientGiftResult(Result);
		return;
	}

	if (Definition->ItemType == TDTags::Item_Type_Quest.GetTag())
	{
		Result.Result = ETDGiftActionResult::QuestItemNotAllowed;
		ClientGiftResult(Result);
		return;
	}

	const int32 AffectionGain =
		NPC->GetGiftAffectionValue(ItemId);

	if (!Inventory->ConsumeItemAt(InventorySlot, 1))
	{
		Result.Result = ETDGiftActionResult::ConsumeFailed;
		ClientGiftResult(Result);
		return;
	}

	Quest->AddAffection(Result.NPCId, AffectionGain);

	if (!Personal->MarkGiftGiven(Result.NPCId))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("선물 소비 후 일일 기록 실패: NPC='%s'"),
			*Result.NPCId.ToString());
	}

	Result.Result = ETDGiftActionResult::Success;
	Result.NPCResponse = NPC->GetGiftThankYouText();
	Result.AffectionGained = AffectionGain;
	Result.TotalAffection =
		Quest->GetAffectionPoints(Result.NPCId);

	ClientGiftResult(Result);

	// 선물 가능 여부를 현재 대화창에 다시 반영한다.
	SendCurrentDialogueLine();
}

TArray<FTDGiftItemView>
UTDInteractionFlowComponent::GetGiftItemViews() const
{
	TArray<FTDGiftItemView> Result;

	const APlayerController* OwningController =
		GetOwnerController();

	const ATDPlayerState* OwningPlayerState =
		OwningController
			? OwningController->GetPlayerState<ATDPlayerState>()
			: nullptr;

	const UTDInventoryComponent* Inventory =
		OwningPlayerState
			? OwningPlayerState->GetInventoryComponent()
			: nullptr;

	const UTDItemUseComponent* ItemUse =
		OwningPlayerState
			? OwningPlayerState->GetItemUseComponent()
			: nullptr;

	if (Inventory == nullptr)
	{
		return Result;
	}

	for (const FTDItemInstance& Item : Inventory->GetItems())
	{
		FTDGiftItemView& View = Result.AddDefaulted_GetRef();

		View.ItemId = Item.ItemId;
		View.SlotIndex = Item.SlotIndex;
		View.Count = Item.Count;
		View.Location = ETDGiftItemLocation::Inventory;

		const FTDItemRow* Definition =
			Inventory->FindItemDefinition(Item.ItemId);

		if (Definition != nullptr)
		{
			View.DisplayName = Definition->DisplayName;
			View.Icon = Definition->Icon;
			View.bCanGift =
				Definition->ItemType != TDTags::Item_Type_Quest.GetTag();
		}
	}

	if (ItemUse != nullptr)
	{
		for (const FTDItemInstance& Item : ItemUse->GetEquippedItems())
		{
			FTDGiftItemView& View = Result.AddDefaulted_GetRef();

			View.ItemId = Item.ItemId;
			View.SlotIndex = Item.SlotIndex;
			View.Count = Item.Count;
			View.Location = ETDGiftItemLocation::Equipped;
			View.bCanGift = false;

			const FTDItemRow* Definition =
				Inventory->FindItemDefinition(Item.ItemId);

			if (Definition != nullptr)
			{
				View.DisplayName = Definition->DisplayName;
				View.Icon = Definition->Icon;
			}
		}
	}

	return Result;
}

void UTDInteractionFlowComponent::EndDialogueSession(
	bool bCompletedNormally)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(
			DialogueDistanceTimerHandle);
	}

	const int32 EndedSessionId = ActiveDialogueSessionId;

	ATDNPCBase* CompletedNPC =
		bCompletedNormally
			? Cast<ATDNPCBase>(ActiveDialogueSource.Get())
			: nullptr;

	ActiveDialogueSessionId = 0;
	ActiveDialogueSource = nullptr;
	ActiveDialogueTable = nullptr;
	ActiveDialoguePawn = nullptr;
	ActiveDialogueRow = NAME_None;

	if (EndedSessionId != 0)
	{
		ClientCloseDialogue(EndedSessionId);
	}

	if (!PendingChapterQuestId.IsNone())
	{
		const FName ChapterQuestId = PendingChapterQuestId;
		PendingChapterQuestId = NAME_None;

		StartChapter(ChapterQuestId);
	}

	if (EndedSessionId != 0 && IsValid(CompletedNPC))
	{
		// 하나의 대화 뒤에 두 서비스 창이 겹치지 않게 강화 NPC를 우선합니다.
		if (CompletedNPC->IsEnhanceNPC())
		{
			if (UTDEnhanceServiceComponent* Enhance =
				GetOwner()->FindComponentByClass<UTDEnhanceServiceComponent>())
			{
				Enhance->StartForNPC(CompletedNPC);
			}
		}
		else if (CompletedNPC->IsShopNPC())
		{
			if (UTDShopServiceComponent* Shop =
				GetOwner()->FindComponentByClass<UTDShopServiceComponent>())
			{
				Shop->StartForNPC(CompletedNPC);
			}
		}
	}
}

void UTDInteractionFlowComponent::RefreshLocalPresentation()
{
	APlayerController* OwningController = GetOwnerController();

	if (!IsValid(OwningController)
		|| !OwningController->IsLocalController())
	{
		return;
	}

	const bool bDialogueOpen = LocalDialogueSessionId != 0;

	// 입력 모드는 대화가 열리거나 닫힐 때만 변경한다.
	// 대사 한 줄이 바뀔 때마다 다시 설정하지 않는다.
	if (bLocalDialogueInputActive != bDialogueOpen)
	{
		if (bDialogueOpen)
		{
			bSavedMouseCursorVisible =
				OwningController->bShowMouseCursor;

			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);

			// 키보드 포커스는 게임 화면에 둔다.
			// 대화 버튼은 마우스로 클릭할 수 있다.
			if (ULocalPlayer* LocalPlayer =
				OwningController->GetLocalPlayer())
			{
				if (UGameViewportClient* ViewportClient =
					LocalPlayer->ViewportClient.Get())
				{
					const TSharedPtr<SViewport> ViewportWidget =
						ViewportClient->GetGameViewportWidget();

					if (ViewportWidget.IsValid())
					{
						InputMode.SetWidgetToFocus(ViewportWidget);
					}
				}
			}

			OwningController->SetInputMode(InputMode);
			OwningController->bShowMouseCursor = true;
		}
		else
		{
			OwningController->SetInputMode(FInputModeGameOnly());
			OwningController->bShowMouseCursor =
				bSavedMouseCursorVisible;
		}

		bLocalDialogueInputActive = bDialogueOpen;
	}

	// 기존처럼 대화/챕터 표시 중에는 NPC 마커를 임시로 숨긴다.
	// 표시가 끝나면 각 NPC가 현재 퀘스트 상태를 다시 반영한다.
	const bool bSuppressMarkers =
		bDialogueOpen || bLocalChapterVisible;

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ATDNPCBase> It(World); It; ++It)
		{
			It->SetQuestMarkerSuppressed(bSuppressMarkers);
		}
	}
}

void UTDInteractionFlowComponent::ClientShowDialogueLine_Implementation(
	int32 SessionId,
	FName DialogueRow,
	FTDDialogueLineView Line)
{
	if (SessionId <= 0)
	{
		return;
	}

	const bool bNewSession =
		LocalDialogueSessionId != SessionId;

	LocalDialogueSessionId = SessionId;

	// F 입력에서 사용할 현재 표시 대사 정보를 기억한다.
	LocalDialogueRow = DialogueRow;
	bLocalShowAcceptDecline = Line.bShowAcceptDecline;

	if (bNewSession)
	{
		if (const APlayerController* OwningController =
			GetOwnerController())
		{
			bEscapeWasDown =
				OwningController->IsInputKeyDown(EKeys::Escape);
		}
		else
		{
			bEscapeWasDown = false;
		}

		SetComponentTickEnabled(true);
	}

	RefreshLocalPresentation();

	OnDialogueLine.Broadcast(
		SessionId,
		DialogueRow,
		Line);
}

void UTDInteractionFlowComponent::ClientCloseDialogue_Implementation(
	int32 SessionId)
{
	if (SessionId <= 0 || LocalDialogueSessionId != SessionId)
	{
		return;
	}

	LocalDialogueSessionId = 0;

	// 종료된 대사의 정보가 다음 상호작용에 남지 않도록 비운다.
	LocalDialogueRow = NAME_None;
	bLocalShowAcceptDecline = false;

	bEscapeWasDown = false;
	SetComponentTickEnabled(false);

	RefreshLocalPresentation();
	OnDialogueClosed.Broadcast(SessionId);
}

void UTDInteractionFlowComponent::ClientFlowQuestActionResult_Implementation(
	FName QuestId,
	ETDQuestActionResult Result)
{
	OnQuestActionResult.Broadcast(QuestId, Result);
}

void UTDInteractionFlowComponent::ClientGiftResult_Implementation(
	FTDGiftResultView Result)
{
	OnGiftResult.Broadcast(Result);
}

void UTDInteractionFlowComponent::ClientShowChapter_Implementation(
	FTDChapterPresentationView Chapter)
{
	bLocalChapterVisible = true;
	RefreshLocalPresentation();

	OnChapterShown.Broadcast(Chapter);
}

void UTDInteractionFlowComponent::ClientCloseChapter_Implementation()
{
	bLocalChapterVisible = false;
	RefreshLocalPresentation();

	OnChapterClosed.Broadcast();
}

bool UTDInteractionFlowComponent::TryHandleDialogueInteractInput()
{
	APlayerController* OwningController = GetOwnerController();

	if (!IsValid(OwningController)
		|| !OwningController->IsLocalController()
		|| LocalDialogueSessionId <= 0)
	{
		// 로컬 화면에 대화가 없으면 기존 일반 상호작용을 허용한다.
		return false;
	}

	if (LocalDialogueRow.IsNone())
	{
		// 대화 중이지만 현재 행 정보가 없다면 아무 작업도 하지 않는다.
		// 같은 입력이 다른 NPC나 상자로 넘어가지는 않게 한다.
		return true;
	}

	if (bLocalShowAcceptDecline)
	{
		// 수락/거절 화면에서는 기존 수락 버튼과 같은 서버 함수를 호출한다.
		ServerAcceptQuest(
			LocalDialogueSessionId,
			LocalDialogueRow);
	}
	else
	{
		// 일반 대사에서는 기존 다음 버튼과 같은 서버 함수를 호출한다.
		ServerAdvanceDialogue(
			LocalDialogueSessionId,
			LocalDialogueRow);
	}

	// 위 서버 호출로 대화가 즉시 종료되더라도 true를 반환한다.
	// 같은 F 입력으로 일반 상호작용이 추가 실행되는 것을 막는다.
	return true;
}
