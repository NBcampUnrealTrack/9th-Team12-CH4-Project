#pragma once

#include "CoreMinimal.h"
#include "Data/TDWorldCondition.h"
#include "Engine/DataTable.h"
#include "TDTreasureChestRow.generated.h"

/**
 * DT_TreasureChest 한 행.
 *
 * RowName이 상자의 영구 ID 역할을 한다.
 * 서로 다른 상자는 반드시 서로 다른 RowName을 사용해야 한다.
 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDTreasureChestRow : public FTableRowBase
{
	GENERATED_BODY()

	/**
	 * DT_ItemDefinition의 RowName.
	 *
	 * 예: ExpPotion, LevelTicket
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Treasure")
	FName RewardItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Treasure",
		meta = (ClampMin = "1"))
	int32 RewardCount = 1;

	/** 획득 연출 후 해당 플레이어 화면에서 사라지기까지 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Treasure",
		meta = (ClampMin = "0.0"))
	float DisappearDelay = 5.0f;

	/** 상자를 볼 수 있고 획득할 수 있는 퀘스트 조건 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Treasure")
	FTDWorldCondition Condition;
};