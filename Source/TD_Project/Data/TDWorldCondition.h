#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TDWorldCondition.generated.h"

/**
 * NPC·상자·퀘스트 오브젝트가 공통으로 사용하는 플레이어 조건.
 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDWorldCondition
{
	GENERATED_BODY()

	/** 플레이어가 모두 가지고 있어야 하는 태그 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Condition")
	FGameplayTagContainer RequiredTags;

	/** 플레이어가 하나라도 가지고 있으면 조건 실패 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Condition")
	FGameplayTagContainer BlockedTags;

	bool IsSatisfiedBy(const FGameplayTagContainer& PlayerTags) const
	{
		return PlayerTags.HasAll(RequiredTags)
			&& !PlayerTags.HasAny(BlockedTags);
	}
};