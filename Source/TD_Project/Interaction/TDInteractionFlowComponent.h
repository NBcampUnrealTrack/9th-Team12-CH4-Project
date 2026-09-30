#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TDChapterRow.h"
#include "Data/TDDialogueRow.h"
#include "Data/TDWorldProgressTypes.h"
#include "TimerManager.h"
#include "TDInteractionFlowComponent.generated.h"

class APlayerController;
class APawn;
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

UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDInteractionFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDInteractionFlowComponent();

	virtual void BeginPlay() override;

	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void BeginDialogueFromSource(
		AActor* Source,
		FName StartRow);

	/**
	 * 기존 상호작용 입력을 현재 대화에서 사용할지 판단한다.
	 *
	 * true:
	 * 대화에서 입력을 사용했다.
	 * 같은 입력으로 일반 NPC/상자/아이템 상호작용을 실행하지 않는다.
	 *
	 * false:
	 * 대화 중이 아니므로 기존 일반 상호작용을 진행한다.
	 */
	bool TryHandleDialogueInteractInput();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Dialogue")
	void ServerAdvanceDialogue(
		int32 SessionId,
		FName ExpectedCurrentRow);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Dialogue")
	void ServerAcceptQuest(
		int32 SessionId,
		FName ExpectedCurrentRow);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Dialogue")
	void ServerDeclineQuest(int32 SessionId);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Dialogue")
	void ServerCancelDialogue(int32 SessionId);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Affection")
	void ServerGiveGift(
		int32 SessionId,
		int32 InventorySlot);

	UFUNCTION(BlueprintPure, Category = "TD|Affection")
	TArray<FTDGiftItemView> GetGiftItemViews() const;

	/**
	 * 기존 전투/스킬 코드와의 연결을 유지하는 함수.
	 *
	 * 이제 대화와 챕터는 게임플레이를 잠그지 않는다.
	 * 사망, 경직, 스킬 자체의 이동 제한 등은 각 시스템이 관리한다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Interaction")
	bool IsGameplayLocked() const
	{
		return false;
	}

	UFUNCTION(BlueprintPure, Category = "TD|Dialogue")
	bool IsDialogueActive() const
	{
		return ActiveDialogueSessionId != 0
			|| LocalDialogueSessionId != 0;
	}
	
	/** 서버 또는 로컬에서 챕터 화면이 진행 중인지 확인합니다. */
	bool IsChapterPresentationActive() const
	{
		return !ActiveChapterId.IsNone()
			|| bLocalChapterVisible;
	}

	UPROPERTY(BlueprintAssignable, Category = "TD|Dialogue")
	FTDFlowOnDialogueLine OnDialogueLine;

	UPROPERTY(BlueprintAssignable, Category = "TD|Dialogue")
	FTDFlowOnDialogueClosed OnDialogueClosed;

	UPROPERTY(BlueprintAssignable, Category = "TD|Quest")
	FTDFlowOnQuestActionResult OnQuestActionResult;

	UPROPERTY(BlueprintAssignable, Category = "TD|Affection")
	FTDFlowOnGiftResult OnGiftResult;

	UPROPERTY(BlueprintAssignable, Category = "TD|Chapter")
	FTDFlowOnChapterShown OnChapterShown;

	UPROPERTY(BlueprintAssignable, Category = "TD|Chapter")
	FTDFlowOnChapterClosed OnChapterClosed;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "TD|Chapter")
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

	// 대화 대상, 플레이어, 거리, 생존 상태를 검사한다.
	bool IsDialogueContextValid() const;

	// 서버에서 대화 중에만 주기적으로 호출한다.
	void CheckDialogueDistance();

	void SendCurrentDialogueLine();
	void EndDialogueSession(bool bCompletedNormally = false);

	bool ApplyDialogueAction(
		const FTDDialogueAction& Action);

	bool ValidateDialogueRequest(
		int32 SessionId,
		FName ExpectedCurrentRow) const;

	// 커서, 입력 모드, NPC 마커만 관리한다.
	// 이동 중단이나 조작 잠금은 하지 않는다.
	void RefreshLocalPresentation();

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

	// 대화를 시작한 캐릭터.
	// 대화 도중 다른 캐릭터로 교체되면 기존 대화를 종료한다.
	UPROPERTY(Transient)
	TObjectPtr<APawn> ActiveDialoguePawn;

	UPROPERTY(Transient)
	TObjectPtr<UTDQuestComponent> BoundQuestComponent;

	FName ActiveDialogueRow = NAME_None;

	int32 ActiveDialogueSessionId = 0;
	int32 DialogueSessionCounter = 0;

	FName PendingChapterQuestId = NAME_None;
	FName ActiveChapterId = NAME_None;

	// 아래 값들은 해당 플레이어의 로컬 화면 상태다.
	int32 LocalDialogueSessionId = 0;

	// 현재 플레이어 화면에 표시된 대사의 행 이름.
	FName LocalDialogueRow = NAME_None;

	// 현재 줄이 수락/거절 선택 화면인지 여부.
	bool bLocalShowAcceptDecline = false;

	bool bLocalChapterVisible = false;
	bool bLocalDialogueInputActive = false;
	bool bSavedMouseCursorVisible = false;
	bool bEscapeWasDown = false;

	FTimerHandle BindRetryTimerHandle;
	FTimerHandle ChapterTimerHandle;
	FTimerHandle DialogueDistanceTimerHandle;
	
};