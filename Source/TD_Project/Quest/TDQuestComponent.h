#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TDQuestTypes.h"
#include "Save/TDPlayerSaveData.h"
#include "TDQuestComponent.generated.h"

class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnQuestListChanged);

/**
 * 플레이어 한 명의 퀘스트 목록과 진행도를 관리한다.
 *
 * PlayerState가 소유하며 퀘스트 정보는 해당 플레이어에게만 복제한다.
 * 모든 변경과 보상 지급은 서버에서만 실행한다.
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDQuestComponent();

	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	ETDQuestActionResult AcceptQuest(FName QuestId);

	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	int32 ReportQuestEvent(FGameplayTag EventTag, int32 Amount = 1);

	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	ETDQuestActionResult TurnInQuest(FName QuestId);

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	FGameplayTag GetQuestStateTag(FName QuestId) const;

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	TArray<FTDQuestViewData> GetQuestViews() const;

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	bool HasQuest(FName QuestId) const;

	UPROPERTY(BlueprintAssignable, Category = "TD|Quest")
	FTDOnQuestListChanged OnQuestListChanged;

	void WriteSaveData(FTDPlayerSaveData& Out) const;
	void ReadSaveData(const FTDPlayerSaveData& In);

protected:
	/** BP_PlayerState의 QuestComponent에서 DT_Quest를 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Quest")
	TObjectPtr<UDataTable> QuestTable;

private:
	const FTDQuestRow* FindQuestDefinition(FName QuestId) const;

	FTDQuestRuntimeData* FindMutableQuest(FName QuestId);
	const FTDQuestRuntimeData* FindQuest(FName QuestId) const;

	bool IsReadyToTurnIn(
		const FTDQuestRuntimeData& Entry,
		const FTDQuestRow& Definition) const;

	void ApplyQuestStateTags(
		const FTDQuestRuntimeData& Entry,
		const FTDQuestRow& Definition);

	bool CanReceiveAllRewards(
		const FTDQuestRow& Definition,
		ETDQuestActionResult& OutFailure) const;

	void NotifyQuestListChanged();

	UFUNCTION()
	void OnRep_QuestEntries();

	UPROPERTY(ReplicatedUsing = OnRep_QuestEntries)
	TArray<FTDQuestRuntimeData> QuestEntries;
};