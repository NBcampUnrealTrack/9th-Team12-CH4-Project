#include "UI/Dialogue/TDDialogueWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TDInteractionFlowComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UTDInteractionFlowComponent* UTDDialogueWidget::GetFlow() const
{
	APlayerController* OwningController = GetOwningPlayer();

	return OwningController
		? OwningController->FindComponentByClass<
			UTDInteractionFlowComponent>()
		: nullptr;
}

void UTDDialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();

	HideQuestWarning();
	SetVisibility(ESlateVisibility::Collapsed);

	if (UTDInteractionFlowComponent* Flow = GetFlow())
	{
		Flow->OnDialogueLine.AddUniqueDynamic(
			this,
			&UTDDialogueWidget::HandleDialogueLine);

		Flow->OnDialogueClosed.AddUniqueDynamic(
			this,
			&UTDDialogueWidget::HandleDialogueClosed);

		Flow->OnQuestActionResult.AddUniqueDynamic(
			this,
			&UTDDialogueWidget::HandleQuestResult);

		Flow->OnGiftResult.AddUniqueDynamic(
			this,
			&UTDDialogueWidget::HandleGiftResult);

		Flow->OnChapterShown.AddUniqueDynamic(
			this,
			&UTDDialogueWidget::HandleChapterShown);

		Flow->OnChapterClosed.AddUniqueDynamic(
			this,
			&UTDDialogueWidget::HandleChapterClosed);
	}
}

void UTDDialogueWidget::NativeDestruct()
{
	// 위젯이 제거될 때 남은 경고 타이머도 정리한다.
	HideQuestWarning();

	if (UTDInteractionFlowComponent* Flow = GetFlow())
	{
		Flow->OnDialogueLine.RemoveDynamic(
			this,
			&UTDDialogueWidget::HandleDialogueLine);

		Flow->OnDialogueClosed.RemoveDynamic(
			this,
			&UTDDialogueWidget::HandleDialogueClosed);

		Flow->OnQuestActionResult.RemoveDynamic(
			this,
			&UTDDialogueWidget::HandleQuestResult);

		Flow->OnGiftResult.RemoveDynamic(
			this,
			&UTDDialogueWidget::HandleGiftResult);

		Flow->OnChapterShown.RemoveDynamic(
			this,
			&UTDDialogueWidget::HandleChapterShown);

		Flow->OnChapterClosed.RemoveDynamic(
			this,
			&UTDDialogueWidget::HandleChapterClosed);
	}

	Super::NativeDestruct();
}

void UTDDialogueWidget::HandleDialogueLine(
	int32 SessionId,
	FName DialogueRow,
	FTDDialogueLineView Line)
{
	CurrentSessionId = SessionId;
	CurrentDialogueRow = DialogueRow;

	// 이전 대사의 경고를 새로운 대사로 가져가지 않는다.
	HideQuestWarning();

	// 빈 영역은 클릭을 가로채지 않는다.
	// 내부의 다음/닫기/수락/선물 버튼은 클릭 가능하다.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	BP_OnShowDialogueLine(Line);
}

void UTDDialogueWidget::HandleDialogueClosed(int32 SessionId)
{
	if (CurrentSessionId != SessionId)
	{
		return;
	}

	HideQuestWarning();

	CurrentSessionId = 0;
	CurrentDialogueRow = NAME_None;

	SetVisibility(
		bChapterVisible
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);

	BP_OnDialogueClosed();
}

void UTDDialogueWidget::HandleQuestResult(
	FName QuestId,
	ETDQuestActionResult Result)
{
	if (Result == ETDQuestActionResult::ActiveSubQuestLimit)
	{
		// 이 결과의 경고는 C++에서 직접 처리한다.
		ShowSubQuestLimitWarning();

		// 기존 BP의 같은 실패 처리와 중복 실행하지 않는다.
		return;
	}

	// 다른 결과는 기존 블루프린트 처리 방식을 유지한다.
	BP_OnQuestActionResult(QuestId, Result);
}

void UTDDialogueWidget::ShowSubQuestLimitWarning()
{
	// 이미 대화가 끝난 화면에는 새 경고를 표시하지 않는다.
	if (CurrentSessionId <= 0)
	{
		return;
	}

	// 경고가 이미 표시 중이면 소리를 다시 겹쳐 재생하지 않는다.
	const bool bShouldPlayWarningSound = !bQuestWarningVisible;
	bQuestWarningVisible = true;

	if (TXT_QuestWarning != nullptr)
	{
		TXT_QuestWarning->SetText(
			NSLOCTEXT(
				"TDDialogue",
				"ActiveSubQuestLimitWarning",
				"서브 퀘스트는 최대 2개까지 받을 수 있습니다."));
	}

	if (Border_QuestWarning != nullptr)
	{
		// 배경과 글자는 보이지만 마우스 입력을 가로채지 않는다.
		Border_QuestWarning->SetVisibility(
			ESlateVisibility::HitTestInvisible);
	}

	if (bShouldPlayWarningSound && QuestWarningSound != nullptr)
	{
		UGameplayStatics::PlaySound2D(
			this,
			QuestWarningSound.Get());
	}

	if (UWorld* World = GetWorld())
	{
		// 다시 수락을 누르면 마지막 경고 시점부터 시간을 다시 센다.
		World->GetTimerManager().ClearTimer(
			QuestWarningTimerHandle);

		World->GetTimerManager().SetTimer(
			QuestWarningTimerHandle,
			this,
			&UTDDialogueWidget::HideQuestWarning,
			FMath::Max(0.1f, QuestWarningDuration),
			false);
	}
}

void UTDDialogueWidget::HideQuestWarning()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			QuestWarningTimerHandle);
	}

	bQuestWarningVisible = false;

	if (Border_QuestWarning != nullptr)
	{
		Border_QuestWarning->SetVisibility(
			ESlateVisibility::Collapsed);
	}
}

void UTDDialogueWidget::HandleGiftResult(
	FTDGiftResultView Result)
{
	BP_OnGiftResult(Result);
}

void UTDDialogueWidget::HandleChapterShown(
	FTDChapterPresentationView Chapter)
{
	bChapterVisible = true;

	// 챕터만 표시 중이면 자식 이미지까지 입력을 가로채지 않는다.
	SetVisibility(
		CurrentSessionId != 0
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::HitTestInvisible);

	BP_OnChapterShown(Chapter);
}

void UTDDialogueWidget::HandleChapterClosed()
{
	bChapterVisible = false;

	// 챕터 표시 중 새 대화가 열렸다면 그 대화는 유지한다.
	SetVisibility(
		CurrentSessionId != 0
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);

	BP_OnChapterClosed();
}

void UTDDialogueWidget::RequestAdvance()
{
	if (CurrentSessionId == 0 || CurrentDialogueRow.IsNone())
	{
		return;
	}

	if (UTDInteractionFlowComponent* Flow = GetFlow())
	{
		Flow->ServerAdvanceDialogue(
			CurrentSessionId,
			CurrentDialogueRow);
	}
}

void UTDDialogueWidget::RequestAcceptQuest()
{
	if (CurrentSessionId == 0 || CurrentDialogueRow.IsNone())
	{
		return;
	}

	if (UTDInteractionFlowComponent* Flow = GetFlow())
	{
		Flow->ServerAcceptQuest(
			CurrentSessionId,
			CurrentDialogueRow);
	}
}

void UTDDialogueWidget::RequestDeclineQuest()
{
	if (CurrentSessionId == 0)
	{
		return;
	}

	if (UTDInteractionFlowComponent* Flow = GetFlow())
	{
		Flow->ServerDeclineQuest(CurrentSessionId);
	}
}

void UTDDialogueWidget::RequestClose()
{
	if (CurrentSessionId == 0)
	{
		return;
	}

	if (UTDInteractionFlowComponent* Flow = GetFlow())
	{
		Flow->ServerCancelDialogue(CurrentSessionId);
	}
}

void UTDDialogueWidget::RequestGiveGift(int32 InventorySlot)
{
	if (CurrentSessionId == 0)
	{
		return;
	}

	if (UTDInteractionFlowComponent* Flow = GetFlow())
	{
		Flow->ServerGiveGift(
			CurrentSessionId,
			InventorySlot);
	}
}

TArray<FTDGiftItemView> UTDDialogueWidget::GetGiftItemViews() const
{
	if (const UTDInteractionFlowComponent* Flow = GetFlow())
	{
		return Flow->GetGiftItemViews();
	}

	return {};
}