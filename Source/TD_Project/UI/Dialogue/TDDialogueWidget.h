#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/TDDialogueRow.h"
#include "Data/TDQuestTypes.h"
#include "TDDialogueWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TD|Dialogue")
	void RequestAdvance();

	UFUNCTION(BlueprintCallable, Category = "TD|Dialogue")
	void RequestClose();

	UFUNCTION(BlueprintPure, Category = "TD|Dialogue")
	int32 GetDialogueSessionId() const
	{
		return CurrentSessionId;
	}

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent,
		Category = "TD|Dialogue",
		DisplayName = "On Show Dialogue Line")
	void BP_OnShowDialogueLine(
		const FTDDialogueLineView& Line);

	UFUNCTION(BlueprintImplementableEvent,
		Category = "TD|Dialogue",
		DisplayName = "On Dialogue Closed")
	void BP_OnDialogueClosed();

	UFUNCTION(BlueprintImplementableEvent,
		Category = "TD|Quest",
		DisplayName = "On Quest Action Result")
	void BP_OnQuestActionResult(
		FName QuestId,
		ETDQuestActionResult Result);

private:
	UFUNCTION()
	void HandleDialogueLineReceived(
		int32 SessionId,
		FName DialogueRow,
		FTDDialogueLineView Line);

	UFUNCTION()
	void HandleDialogueClosed(int32 SessionId);

	UFUNCTION()
	void HandleQuestActionResult(
		FName QuestId,
		ETDQuestActionResult Result);

	int32 CurrentSessionId = 0;
	FName CurrentDialogueRow;
};