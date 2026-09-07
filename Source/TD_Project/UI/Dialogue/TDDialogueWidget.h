#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/TDChapterRow.h"
#include "Data/TDDialogueRow.h"
#include "Data/TDWorldProgressTypes.h"
#include "TDDialogueWidget.generated.h"

class UTDInteractionFlowComponent;

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDDialogueWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable,
		Category = "TD|Dialogue")
	void RequestAdvance();

	UFUNCTION(BlueprintCallable,
		Category = "TD|Dialogue")
	void RequestAcceptQuest();

	UFUNCTION(BlueprintCallable,
		Category = "TD|Dialogue")
	void RequestDeclineQuest();

	UFUNCTION(BlueprintCallable,
		Category = "TD|Dialogue")
	void RequestClose();

	UFUNCTION(BlueprintCallable,
		Category = "TD|Affection")
	void RequestGiveGift(int32 InventorySlot);

	UFUNCTION(BlueprintPure,
		Category = "TD|Affection")
	TArray<FTDGiftItemView>
	GetGiftItemViews() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent,
		DisplayName = "On Show Dialogue Line")
	void BP_OnShowDialogueLine(
		const FTDDialogueLineView& Line);

	UFUNCTION(BlueprintImplementableEvent,
		DisplayName = "On Dialogue Closed")
	void BP_OnDialogueClosed();

	UFUNCTION(BlueprintImplementableEvent,
		DisplayName = "On Quest Action Result")
	void BP_OnQuestActionResult(
		FName QuestId,
		ETDQuestActionResult Result);

	UFUNCTION(BlueprintImplementableEvent,
		DisplayName = "On Gift Result")
	void BP_OnGiftResult(
		const FTDGiftResultView& Result);

	UFUNCTION(BlueprintImplementableEvent,
		DisplayName = "On Chapter Shown")
	void BP_OnChapterShown(
		const FTDChapterPresentationView& Chapter);

	UFUNCTION(BlueprintImplementableEvent,
		DisplayName = "On Chapter Closed")
	void BP_OnChapterClosed();

private:
	UTDInteractionFlowComponent* GetFlow() const;

	UFUNCTION()
	void HandleDialogueLine(
		int32 SessionId,
		FName DialogueRow,
		FTDDialogueLineView Line);

	UFUNCTION()
	void HandleDialogueClosed(int32 SessionId);

	UFUNCTION()
	void HandleQuestResult(
		FName QuestId,
		ETDQuestActionResult Result);

	UFUNCTION()
	void HandleGiftResult(
		FTDGiftResultView Result);

	UFUNCTION()
	void HandleChapterShown(
		FTDChapterPresentationView Chapter);

	UFUNCTION()
	void HandleChapterClosed();

	int32 CurrentSessionId = 0;
	FName CurrentDialogueRow;
	bool bChapterVisible = false;
};