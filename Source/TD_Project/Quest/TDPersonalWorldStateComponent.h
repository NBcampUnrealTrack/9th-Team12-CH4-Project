#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TDWorldCondition.h"
#include "GameplayTagContainer.h"
#include "Save/TDPlayerSaveData.h"
#include "TDPersonalWorldStateComponent.generated.h"

/**
 * 퀘스트 또는 개인 월드 상태가 변경됐을 때.
 *
 * NPC·상자·퀘스트 UI가 이 이벤트를 받아 자기 표시 상태를 갱신한다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnPersonalWorldStateChanged);

/**
 * 플레이어마다 다른 퀘스트 및 월드 상태.
 *
 * PlayerState가 소유하므로 캐릭터가 죽고 부활해도 유지된다.
 * 다른 플레이어에게는 필요하지 않으므로 값은 OwnerOnly로 복제한다.
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDPersonalWorldStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDPersonalWorldStateComponent();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── 조회 ──────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "TD|Quest")
	FGameplayTagContainer GetQuestProgressTags() const
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

	UFUNCTION(BlueprintPure, Category = "TD|World")
	bool HasClaimedChest(FName ChestId) const;

	UFUNCTION(BlueprintPure, Category = "TD|World")
	const TArray<FName>& GetClaimedChestIds() const
	{
		return ClaimedChestIds;
	}

	// ── 서버 전용 변경 ────────────────────────────────────

	/**
	 * 퀘스트 진행 태그를 추가한다.
	 * 클라이언트에서 직접 호출하면 실패한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	bool AddQuestTag(FGameplayTag Tag);

	/**
	 * 퀘스트 진행 태그를 제거한다.
	 * 클라이언트에서 직접 호출하면 실패한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TD|Quest",
		meta = (BlueprintAuthorityOnly = "true"))
	bool RemoveQuestTag(FGameplayTag Tag);

	/**
	 * 상자를 획득 완료 상태로 만든다.
	 *
	 * 이미 획득한 상자면 false를 반환하므로 중복 보상을 막는 데 사용한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TD|World",
		meta = (BlueprintAuthorityOnly = "true"))
	bool MarkChestClaimed(FName ChestId);

	// ── 저장 데이터 ───────────────────────────────────────

	void WriteSaveData(FTDPlayerSaveData& Out) const;
	void ReadSaveData(const FTDPlayerSaveData& In);

	/** 서버 상태 변경과 클라이언트 복제 수신 양쪽에서 발생한다. */
	UPROPERTY(BlueprintAssignable, Category = "TD|World")
	FTDOnPersonalWorldStateChanged OnPersonalWorldStateChanged;

private:
	void NotifyStateChanged();

	UFUNCTION()
	void OnRep_QuestProgressTags();

	UFUNCTION()
	void OnRep_ClaimedChestIds();

	/**
	 * 현재 퀘스트 단계와 완료 상태.
	 *
	 * 다른 플레이어에게 보낼 필요가 없으므로 OwnerOnly로 복제한다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_QuestProgressTags)
	FGameplayTagContainer QuestProgressTags;

	/**
	 * 이 플레이어가 이미 획득한 영구 상자 ID 목록.
	 *
	 * 상자 Actor 이름이 아니라 DT_TreasureChest의 고정 RowName을 저장한다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_ClaimedChestIds)
	TArray<FName> ClaimedChestIds;
};