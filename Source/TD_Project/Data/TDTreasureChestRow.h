#pragma once

#include "CoreMinimal.h"
#include "Data/TDWorldCondition.h"
#include "Data/TDWorldProgressTypes.h"
#include "Engine/DataTable.h"
#include "TDTreasureChestRow.generated.h"

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDChestItemReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure",
		meta = (ClampMin = "1"))
	int32 Count = 1;
};

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDChestRandomReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure",
		meta = (ClampMin = "1"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure",
		meta = (ClampMin = "1"))
	int32 MaxCount = 1;

	/** 숫자가 클수록 뽑힐 확률이 높다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure",
		meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
};

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDTreasureChestRow
	: public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure")
	ETDChestResetType ResetType =
		ETDChestResetType::OneTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure")
	TArray<FTDChestItemReward> FixedItemRewards;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure",
		meta = (ClampMin = "0"))
	int32 FixedGold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure",
		meta = (ClampMin = "0"))
	int32 FixedExp = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure")
	bool bUseRandomRewards = false;

	/**
	 * 독립 추첨 횟수.
	 * 최종 확정 규칙에 따라 최대 2로 제한된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure|Random",
		meta = (ClampMin = "0", ClampMax = "2"))
	int32 MinRollCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure|Random",
		meta = (ClampMin = "0", ClampMax = "2"))
	int32 MaxRollCount = 2;

	/**
	 * 꽝의 가중치.
	 * 0이면 꽝이 없고, 값이 클수록 꽝 확률이 높다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure|Random",
		meta = (ClampMin = "0.0"))
	float MissWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure|Random")
	TArray<FTDChestRandomReward> RandomRewards;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure",
		meta = (ClampMin = "0.0"))
	float DisappearDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure")
	FTDWorldCondition Condition;
};