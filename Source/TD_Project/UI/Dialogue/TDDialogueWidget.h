#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/TDChapterRow.h"
#include "Data/TDDialogueRow.h"
#include "Data/TDQuestTypes.h"
#include "Data/TDWorldProgressTypes.h"
#include "TimerManager.h"
#include "TDDialogueWidget.generated.h"

class UBorder;
class USoundBase;
class UTextBlock;
class UTDInteractionFlowComponent;

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TD|Dialogue")
	void RequestAdvance();

	UFUNCTION(BlueprintCallable, Category = "TD|Dialogue")
	void RequestAcceptQuest();

	UFUNCTION(BlueprintCallable, Category = "TD|Dialogue")
	void RequestDeclineQuest();

	UFUNCTION(BlueprintCallable, Category = "TD|Dialogue")
	void RequestClose();

	UFUNCTION(BlueprintCallable, Category = "TD|Affection")
	void RequestGiveGift(int32 InventorySlot);

	UFUNCTION(BlueprintPure, Category = "TD|Affection")
	TArray<FTDGiftItemView> GetGiftItemViews() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(
		BlueprintImplementableEvent,
		DisplayName = "On Show Dialogue Line")
	void BP_OnShowDialogueLine(
		const FTDDialogueLineView& Line);

	UFUNCTION(
		BlueprintImplementableEvent,
		DisplayName = "On Dialogue Closed")
	void BP_OnDialogueClosed();

	UFUNCTION(
		BlueprintImplementableEvent,
		DisplayName = "On Quest Action Result")
	void BP_OnQuestActionResult(
		FName QuestId,
		ETDQuestActionResult Result);

	UFUNCTION(
		BlueprintImplementableEvent,
		DisplayName = "On Gift Result")
	void BP_OnGiftResult(
		const FTDGiftResultView& Result);

	UFUNCTION(
		BlueprintImplementableEvent,
		DisplayName = "On Chapter Shown")
	void BP_OnChapterShown(
		const FTDChapterPresentationView& Chapter);

	UFUNCTION(
		BlueprintImplementableEvent,
		DisplayName = "On Chapter Closed")
	void BP_OnChapterClosed();

	// WBP_Dialogue의 Class Defaults에서 경고음을 지정한다.
	UPROPERTY(EditDefaultsOnly, Category = "TD|Dialogue|Warning")
	TObjectPtr<USoundBase> QuestWarningSound = nullptr;

	// 경고 문구를 화면에 표시할 시간.
	UPROPERTY(
		EditDefaultsOnly,
		Category = "TD|Dialogue|Warning",
		meta = (ClampMin = "0.1", UIMin = "0.1"))
	float QuestWarningDuration = 2.0f;

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

	// 서브 퀘스트 최대 개수 경고를 표시한다.
	void ShowSubQuestLimitWarning();

	// 경고만 숨긴다. 대화창 자체는 닫지 않는다.
	void HideQuestWarning();

	// WBP_Dialogue Designer에 같은 이름의 Border를 만든다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_QuestWarning = nullptr;

	// 위 Border 안에 같은 이름의 Text를 만든다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_QuestWarning = nullptr;

	int32 CurrentSessionId = 0;
	FName CurrentDialogueRow = NAME_None;
	bool bChapterVisible = false;

	bool bQuestWarningVisible = false;
	FTimerHandle QuestWarningTimerHandle;
};