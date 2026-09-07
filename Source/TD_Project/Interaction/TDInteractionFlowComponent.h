#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TDChapterRow.h"
#include "Data/TDDialogueRow.h"
#include "Data/TDWorldProgressTypes.h"
#include "TDInteractionFlowComponent.generated.h"

class APlayerController;
class UDataTable;
class UTDQuestComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FTDFlowOnDialogueLine,
	int32, SessionId,
	FName, DialogueRow,
	FTDDialogueLineView, Line);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTDFlowOnDialogueClosed,
	int32, SessionId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FTDFlowOnQuestActionResult,
	FName, QuestId,
	ETDQuestActionResult, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTDFlowOnGiftResult,
	FTDGiftResultView, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTDFlowOnChapterShown,
	FTDChapterPresentationView, Chapter);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
	FTDFlowOnChapterClosed);

UCLASS(ClassGroup = (TD),
	meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDInteractionFlowComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UTDInteractionFlowComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(
		const EEndPlayReason::Type
			EndPlayReason) override;

	void BeginDialogueFromSource(
		AActor* Source,
		FName StartRow);

	UFUNCTION(Server, Reliable,
		BlueprintCallable,
		Category = "TD|Dialogue")
	void ServerAdvanceDialogue(
		int32 SessionId,
		FName ExpectedCurrentRow);

	UFUNCTION(Server, Reliable,
		BlueprintCallable,
		Category = "TD|Dialogue")
	void ServerAcceptQuest(
		int32 SessionId,
		FName ExpectedCurrentRow);

	UFUNCTION(Server, Reliable,
		BlueprintCallable,
		Category = "TD|Dialogue")
	void ServerDeclineQuest(int32 SessionId);

	UFUNCTION(Server, Reliable,
		BlueprintCallable,
		Category = "TD|Dialogue")
	void ServerCancelDialogue(int32 SessionId);

	UFUNCTION(Server, Reliable,
		BlueprintCallable,
		Category = "TD|Affection")
	void ServerGiveGift(
		int32 SessionId,
		int32 InventorySlot);

	UFUNCTION(BlueprintPure,
		Category = "TD|Affection")
	TArray<FTDGiftItemView>
	GetGiftItemViews() const;

	UFUNCTION(BlueprintPure,
		Category = "TD|Interaction")
	bool IsGameplayLocked() const
	{
		return bGameplayLocked;
	}

	UPROPERTY(BlueprintAssignable,
		Category = "TD|Dialogue")
	FTDFlowOnDialogueLine OnDialogueLine;

	UPROPERTY(BlueprintAssignable,
		Category = "TD|Dialogue")
	FTDFlowOnDialogueClosed OnDialogueClosed;

	UPROPERTY(BlueprintAssignable,
		Category = "TD|Quest")
	FTDFlowOnQuestActionResult
		OnQuestActionResult;

	UPROPERTY(BlueprintAssignable,
		Category = "TD|Affection")
	FTDFlowOnGiftResult OnGiftResult;

	UPROPERTY(BlueprintAssignable,
		Category = "TD|Chapter")
	FTDFlowOnChapterShown OnChapterShown;

	UPROPERTY(BlueprintAssignable,
		Category = "TD|Chapter")
	FTDFlowOnChapterClosed OnChapterClosed;

protected:
	UPROPERTY(EditDefaultsOnly,
		Category = "TD|Chapter")
	TObjectPtr<UDataTable> ChapterTable;

private:
	APlayerController* GetOwnerController() const;
	UTDQuestComponent* GetQuestComponent() const;

	void TryBindQuestComponent();

	UFUNCTION()
	void HandleQuestListChanged();

	void TryStartCurrentChapter();
	void StartChapter(FName QuestId);
	void FinishChapter();

	const FTDChapterRow* FindChapterForQuest(
		FName QuestId,
		FName& OutChapterId) const;

	void SetGameplayLocked(bool bLocked);

	UFUNCTION(Client, Reliable)
	void ClientSetGameplayLocked(bool bLocked);

	void SendCurrentDialogueLine();
	void EndDialogueSession();

	bool ApplyDialogueAction(
		const FTDDialogueAction& Action);

	bool ValidateDialogueRequest(
		int32 SessionId,
		FName ExpectedCurrentRow) const;

	UFUNCTION(Client, Reliable)
	void ClientShowDialogueLine(
		int32 SessionId,
		FName DialogueRow,
		FTDDialogueLineView Line);

	UFUNCTION(Client, Reliable)
	void ClientCloseDialogue(int32 SessionId);

	UFUNCTION(Client, Reliable)
	void ClientFlowQuestActionResult(
		FName QuestId,
		ETDQuestActionResult Result);

	UFUNCTION(Client, Reliable)
	void ClientGiftResult(
		FTDGiftResultView Result);

	UFUNCTION(Client, Reliable)
	void ClientShowChapter(
		FTDChapterPresentationView Chapter);

	UFUNCTION(Client, Reliable)
	void ClientCloseChapter();

	UPROPERTY(Transient)
	TObjectPtr<AActor> ActiveDialogueSource;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ActiveDialogueTable;

	UPROPERTY(Transient)
	TObjectPtr<UTDQuestComponent>
		BoundQuestComponent;

	FName ActiveDialogueRow;
	int32 ActiveDialogueSessionId = 0;
	int32 DialogueSessionCounter = 0;

	FName PendingChapterQuestId;
	FName ActiveChapterId;

	bool bGameplayLocked = false;

	FTimerHandle BindRetryTimerHandle;
	FTimerHandle ChapterTimerHandle;
};