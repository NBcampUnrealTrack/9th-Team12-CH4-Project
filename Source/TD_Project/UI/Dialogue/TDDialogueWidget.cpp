#include "UI/Dialogue/TDDialogueWidget.h"

#include "Player/TDPlayerController.h"

void UTDDialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::Collapsed);

	if (ATDPlayerController* Controller =
		Cast<ATDPlayerController>(GetOwningPlayer()))
	{
		Controller->OnDialogueLineReceived.AddUniqueDynamic(
			this,
			&UTDDialogueWidget::HandleDialogueLineReceived);

		Controller->OnDialogueClosed.AddUniqueDynamic(
			this,
			&UTDDialogueWidget::HandleDialogueClosed);

		Controller->OnQuestActionResult.AddUniqueDynamic(
			this,
			&UTDDialogueWidget::HandleQuestActionResult);
	}
}

void UTDDialogueWidget::NativeDestruct()
{
	if (ATDPlayerController* Controller =
		Cast<ATDPlayerController>(GetOwningPlayer()))
	{
		Controller->OnDialogueLineReceived.RemoveDynamic(
			this,
			&UTDDialogueWidget::HandleDialogueLineReceived);

		Controller->OnDialogueClosed.RemoveDynamic(
			this,
			&UTDDialogueWidget::HandleDialogueClosed);

		Controller->OnQuestActionResult.RemoveDynamic(
			this,
			&UTDDialogueWidget::HandleQuestActionResult);
	}

	Super::NativeDestruct();
}

void UTDDialogueWidget::HandleDialogueLineReceived(
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
	if (SessionId != CurrentSessionId)
	{
		return;
	}

	CurrentSessionId = 0;
	CurrentDialogueRow = NAME_None;

	SetVisibility(ESlateVisibility::Collapsed);
	BP_OnDialogueClosed();
}

void UTDDialogueWidget::HandleQuestActionResult(
	FName QuestId,
	ETDQuestActionResult Result)
{
	BP_OnQuestActionResult(
		QuestId,
		Result);
}

void UTDDialogueWidget::RequestAdvance()
{
	if (CurrentSessionId == 0
		|| CurrentDialogueRow.IsNone())
	{
		return;
	}

	if (ATDPlayerController* Controller =
		Cast<ATDPlayerController>(GetOwningPlayer()))
	{
		Controller->ServerAdvanceDialogue(
			CurrentSessionId,
			CurrentDialogueRow);
	}
}

void UTDDialogueWidget::RequestClose()
{
	if (CurrentSessionId == 0)
	{
		return;
	}

	if (ATDPlayerController* Controller =
		Cast<ATDPlayerController>(GetOwningPlayer()))
	{
		Controller->ServerCancelDialogue(
			CurrentSessionId);
	}
}