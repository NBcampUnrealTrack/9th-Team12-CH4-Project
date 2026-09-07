#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TDQuestTypes.h"
#include "Save/TDPlayerSaveData.h"
#include "TDQuestComponent.generated.h"

class UDataTable;
class UTDInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnQuestListChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FTDOnAffectionChanged,
	FName, NPCId,
	int32, NewPoints);

UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDQuestComponent();

	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── 수락·완료·포기 ───────────────────────────────────

	/**
	 * 대상이 없는 자동 퀘스트 또는 기존 데이터 호환용.
	 * NPC/물건 퀘스트는 AcceptQuestAtTarget을 사용한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	ETDQuestActionResult AcceptQuest(FName QuestId);

	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	ETDQuestActionResult AcceptQuestAtTarget(
		FName QuestId,
		ETDQuestTargetType TargetType,
		FName TargetId);

	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	ETDQuestActionResult TurnInQuest(FName QuestId);

	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	ETDQuestActionResult TurnInQuestAtTarget(
		FName QuestId,
		ETDQuestTargetType TargetType,
		FName TargetId);

	UFUNCTION(Server, Reliable, BlueprintCallable,
		Category = "TD|Quest")
	void ServerAbandonQuest(FName QuestId);

	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	ETDQuestActionResult AbandonQuest(FName QuestId);

	// ── 진행도 입력 ───────────────────────────────────────

	/** 기존 대화 이벤트 방식과 호환 */
	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	int32 ReportQuestEvent(
		FGameplayTag EventTag,
		int32 Amount = 1);

	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	int32 ReportMonsterKilled(FName MonsterId);

	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	int32 ReportZoneEntered(FGameplayTag ZoneId);

	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	int32 ReportChestOpened(FName ChestId);

	/**
	 * 아이템 목표는 누적 증가가 아니다.
	 * 인벤토리의 현재 보유량을 다시 읽어 계산한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	int32 RecalculateInventoryObjectives();

	// ── 조회 ──────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	bool HasQuest(FName QuestId) const;

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	bool HasCompletedQuest(FName QuestId) const;

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	FGameplayTag GetQuestStateTag(FName QuestId) const;

	/** 퀘스트 창: 진행 중인 퀘스트만 반환 */
	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	TArray<FTDQuestViewData> GetQuestViews() const;

	/** HUD: 메인 1개 + 수락 순서 서브/일일 2개 */
	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	TArray<FTDQuestViewData> GetQuestTrackerViews() const;

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	int32 GetActiveSubQuestCount() const;

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	ETDQuestActionResult GetQuestAcceptResult(
		FName QuestId,
		bool bIgnoreSubQuestLimit = false) const;

	const FTDQuestRow* GetQuestDefinition(
		FName QuestId) const;

	// ── NPC·물건에서 사용할 우선순위 조회 ────────────────

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	FName FindBestTurnInQuestForTarget(
		ETDQuestTargetType TargetType,
		FName TargetId) const;

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	FName FindBestOfferQuestForTarget(
		ETDQuestTargetType TargetType,
		FName TargetId,
		bool bIgnoreSubQuestLimitForMarker = true) const;

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	FTDQuestMarkerView GetQuestMarkerForTarget(
		ETDQuestTargetType TargetType,
		FName TargetId) const;

	// ── 호감도 ────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "TD|Affection")
	int32 GetAffectionPoints(FName NPCId) const;

	UFUNCTION(BlueprintPure, Category = "TD|Affection")
	FText GetAffectionTierText(FName NPCId) const;

	UFUNCTION(BlueprintCallable, Category = "TD|Affection",
		meta = (BlueprintAuthorityOnly = "true"))
	bool AddAffection(FName NPCId, int32 Amount);

	// ── 날짜 ──────────────────────────────────────────────

	/** 한국 날짜를 YYYYMMDD 정수로 반환 */
	UFUNCTION(BlueprintPure, Category = "TD|Daily")
	static int32 GetCurrentKstDayKey();

	// ── 알림 ──────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "TD|Quest")
	FTDOnQuestListChanged OnQuestListChanged;

	UPROPERTY(BlueprintAssignable, Category = "TD|Affection")
	FTDOnAffectionChanged OnAffectionChanged;

	// ── 저장 ──────────────────────────────────────────────

	void WriteSaveData(FTDPlayerSaveData& Out) const;
	void ReadSaveData(const FTDPlayerSaveData& In);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "TD|Quest")
	TObjectPtr<UDataTable> QuestTable;

private:
	const FTDQuestRow* FindQuestDefinition(
		FName QuestId) const;

	FTDQuestRuntimeData* FindMutableQuest(
		FName QuestId);

	const FTDQuestRuntimeData* FindQuest(
		FName QuestId) const;

	FTDAffectionRuntimeData* FindMutableAffection(
		FName NPCId);

	const FTDAffectionRuntimeData* FindAffection(
		FName NPCId) const;

	ETDQuestActionResult AcceptQuestInternal(
		FName QuestId,
		bool bValidateTarget,
		ETDQuestTargetType TargetType,
		FName TargetId);

	ETDQuestActionResult TurnInQuestInternal(
		FName QuestId,
		bool bValidateTarget,
		ETDQuestTargetType TargetType,
		FName TargetId);

	ETDQuestActionResult CheckAcceptConditions(
		FName QuestId,
		bool bIgnoreSubQuestLimit) const;

	bool IsReadyToTurnIn(
		const FTDQuestRuntimeData& Entry,
		const FTDQuestRow& Definition) const;

	void InitializeObjectiveProgress(
		FTDQuestRuntimeData& Entry,
		const FTDQuestRow& Definition) const;

	bool RefreshQuestState(
		FTDQuestRuntimeData& Entry,
		const FTDQuestRow& Definition);

	void ApplyQuestStateTags(
		const FTDQuestRuntimeData& Entry,
		const FTDQuestRow& Definition);

	void RemoveQuestStateTags(
		const FTDQuestRow& Definition);

	bool CanApplyTurnInInventoryTransaction(
		const FTDQuestRow& Definition,
		ETDQuestActionResult& OutFailure) const;

	bool ApplyTurnInInventoryTransaction(
		const FTDQuestRow& Definition);

	void EnsureInitialMainQuest();
	void RepairDuplicateActiveMainQuests();
	void ProcessAutomaticQuests();

	void NotifyQuestListChanged();

	FTDQuestViewData MakeQuestView(
		const FTDQuestRuntimeData& Entry,
		const FTDQuestRow& Definition) const;

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleZoneChanged(FGameplayTag NewZoneId);

	UFUNCTION()
	void HandleCharacterSelected();

	UFUNCTION()
	void OnRep_QuestEntries();

	UFUNCTION()
	void OnRep_AffectionEntries();

	UPROPERTY(ReplicatedUsing = OnRep_QuestEntries)
	TArray<FTDQuestRuntimeData> QuestEntries;

	UPROPERTY(ReplicatedUsing = OnRep_AffectionEntries)
	TArray<FTDAffectionRuntimeData> AffectionEntries;

	int64 NextAcceptSequence = 1;

	/** 제출 처리 중 인벤토리 콜백이 Ready 상태를 되돌리는 것을 막는다. */
	bool bApplyingTurnInTransaction = false;
};