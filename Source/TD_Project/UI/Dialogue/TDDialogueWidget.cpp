#include "UI/Dialogue/TDDialogueWidget.h"

#include "GameFramework/PlayerController.h"
#include "Interaction/TDInteractionFlowComponent.h"

UTDInteractionFlowComponent*
UTDDialogueWidget::GetFlow() const
{
	APlayerController* Controller =
		GetOwningPlayer();

	return Controller
		? Controller->FindComponentByClass<
			UTDInteractionFlowComponent>()
		: nullptr;
}

void UTDDialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();

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

	SetVisibility(ESlateVisibility::Visible);
	BP_OnShowDialogueLine(Line);
}

void UTDDialogueWidget::HandleDialogueClosed(
	int32 SessionId)
{
	if (CurrentSessionId != SessionId)
	{
		return;
	}

	CurrentSessionId = 0;
	CurrentDialogueRow = NAME_None;

	if (!bChapterVisible)
	{
		SetVisibility(
			ESlateVisibility::Collapsed);
	}

	BP_OnDialogueClosed();
}

void UTDDialogueWidget::HandleQuestResult(
	FName QuestId,
	ETDQuestActionResult Result)
{
	BP_OnQuestActionResult(
		QuestId,
		Result);
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

	SetVisibility(ESlateVisibility::Visible);
	BP_OnChapterShown(Chapter);
}

void UTDDialogueWidget::HandleChapterClosed()
{
	bChapterVisible = false;

	if (CurrentSessionId == 0)
	{
		SetVisibility(
			ESlateVisibility::Collapsed);
	}

	BP_OnChapterClosed();
}

void UTDDialogueWidget::RequestAdvance()
{
	if (CurrentSessionId == 0
		|| CurrentDialogueRow.IsNone())
	{
		return;
	}

	if (UTDInteractionFlowComponent* Flow =
		GetFlow())
	{
		Flow->ServerAdvanceDialogue(
			CurrentSessionId,
			CurrentDialogueRow);
	}
}

void UTDDialogueWidget::RequestAcceptQuest()
{
	if (CurrentSessionId == 0
		|| CurrentDialogueRow.IsNone())
	{
		return;
	}

	if (UTDInteractionFlowComponent* Flow =
		GetFlow())
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

	if (UTDInteractionFlowComponent* Flow =
		GetFlow())
	{
		Flow->ServerDeclineQuest(
			CurrentSessionId);
	}
}

void UTDDialogueWidget::RequestClose()
{
	if (CurrentSessionId == 0)
	{
		return;
	}

	if (UTDInteractionFlowComponent* Flow =
		GetFlow())
	{
		Flow->ServerCancelDialogue(
			CurrentSessionId);
	}
}

void UTDDialogueWidget::RequestGiveGift(
	int32 InventorySlot)
{
	if (CurrentSessionId == 0)
	{
		return;
	}

	if (UTDInteractionFlowComponent* Flow =
		GetFlow())
	{
		Flow->ServerGiveGift(
			CurrentSessionId,
			InventorySlot);
	}
}

TArray<FTDGiftItemView>
UTDDialogueWidget::GetGiftItemViews() const
{
	if (const UTDInteractionFlowComponent* Flow =
		GetFlow())
	{
		return Flow->GetGiftItemViews();
	}

	return {};
}