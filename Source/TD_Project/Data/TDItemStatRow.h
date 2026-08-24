#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Stats/TDStatTypes.h"
#include "TDItemStatRow.generated.h"

/**
 * DT_ItemStat 의 행. 아이템이 고정으로 주는 스탯 하나다.
 *
 * DT_ItemDefinition 안에 TArray<FTDStatModifier> 로 넣으면 중첩 배열이 되어
 * CSV 로 편집할 수 없게 된다. 그래서 테이블을 분리하고 ItemId 로만 가리킨다.
 * 아이템 하나가 여러 스탯을 주면 그 수만큼 행이 생긴다.
 *
 *   RowName            ItemId      StatTag                       Op         Value
 *   RingFire_Damage    Ring_Fire   Stat.Offense.Damage.Magical   Added      10
 *   RingFire_Mana      Ring_Fire   Stat.Resource.Mana.Max        Increased  0.05
 */
USTRUCT(BlueprintType)
struct FTDItemStatRow : public FTableRowBase
{
	GENERATED_BODY()

	/** DT_ItemDefinition 의 RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Stat")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Stat")
	FGameplayTag StatTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Stat")
	ETDModOp Op = ETDModOp::Added;

	/** Increased/More 는 비율값. 20% 증가는 0.2 로 넣는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Stat")
	float Value = 0.f;
};
