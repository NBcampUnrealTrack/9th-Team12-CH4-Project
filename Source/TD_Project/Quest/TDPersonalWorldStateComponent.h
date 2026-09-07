#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TDWorldCondition.h"
#include "Data/TDWorldProgressTypes.h"
#include "GameplayTagContainer.h"
#include "Save/TDPlayerSaveData.h"
#include "TDPersonalWorldStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
	FTDOnPersonalWorldStateChanged);

UCLASS(ClassGroup = (TD),
	meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDPersonalWorldStateComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UTDPersonalWorldStateComponent();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>&
			OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	FGameplayTagContainer
	GetQuestProgressTags() const
	{
		return QuestProgressTags;
	}

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	bool HasQuestTag(
		FGameplayTag Tag,
		bool bExactMatch = true) const;

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	bool MatchesCondition(
		const FTDWorldCondition& Condition) const;

	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	bool AddQuestTag(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	bool RemoveQuestTag(FGameplayTag Tag);

	// ── 상자 ─────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "TD|Treasure")
	bool HasClaimedChest(FName ChestId) const;

	UFUNCTION(BlueprintPure, Category = "TD|Treasure")
	bool CanClaimChest(
		FName ChestId,
		ETDChestResetType ResetType) const;

	UFUNCTION(BlueprintCallable, Category = "TD|Treasure",
		meta = (BlueprintAuthorityOnly = "true"))
	bool MarkChestClaimed(FName ChestId);

	UFUNCTION(BlueprintCallable, Category = "TD|Treasure",
		meta = (BlueprintAuthorityOnly = "true"))
	bool MarkChestClaimedWithReset(
		FName ChestId,
		ETDChestResetType ResetType);

	// ── 챕터 ─────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "TD|Chapter")
	bool HasSeenChapter(FName ChapterId) const;

	UFUNCTION(BlueprintCallable, Category = "TD|Chapter",
		meta = (BlueprintAuthorityOnly = "true"))
	bool MarkChapterSeen(FName ChapterId);

	// ── NPC 선물 ─────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "TD|Affection")
	bool CanGiftToNPC(FName NPCId) const;

	UFUNCTION(BlueprintCallable, Category = "TD|Affection",
		meta = (BlueprintAuthorityOnly = "true"))
	bool MarkGiftGiven(FName NPCId);

	void WriteSaveData(FTDPlayerSaveData& Out) const;
	void ReadSaveData(const FTDPlayerSaveData& In);

	UPROPERTY(BlueprintAssignable, Category = "TD|World")
	FTDOnPersonalWorldStateChanged
	OnPersonalWorldStateChanged;

private:
	void NotifyStateChanged();

	const FTDChestClaimRecord* FindChestClaim(
		FName ChestId) const;

	FTDChestClaimRecord* FindMutableChestClaim(
		FName ChestId);

	const FTDNpcGiftRecord* FindGiftRecord(
		FName NPCId) const;

	FTDNpcGiftRecord* FindMutableGiftRecord(
		FName NPCId);

	UFUNCTION()
	void OnRep_State();

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FGameplayTagContainer QuestProgressTags;

	/** 구버전과 BP 호환용 일회성 상자 목록 */
	UPROPERTY(ReplicatedUsing = OnRep_State)
	TArray<FName> ClaimedChestIds;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	TArray<FTDChestClaimRecord> ChestClaimRecords;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	TArray<FName> SeenChapterIds;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	TArray<FTDNpcGiftRecord> NpcGiftRecords;
};