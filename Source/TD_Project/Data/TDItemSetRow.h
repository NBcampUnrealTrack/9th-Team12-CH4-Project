#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Stats/TDStatTypes.h"
#include "TDItemSetRow.generated.h"

/**
 * DT_ItemSet 의 행. 세트 하나의 표시 정보다.
 *
 * 식별자는 RowName 이며 FTDItemRow::SetId 와 짝이다.
 * 효과는 여기 없고 DT_ItemSetBonus 에 단계별로 나뉘어 있다 —
 * 한 세트가 여러 단계를 가지므로 행에 담으면 중첩 배열이 된다.
 */
USTRUCT(BlueprintType)
struct FTDItemSetRow : public FTableRowBase
{
	GENERATED_BODY()

	/** "화염술사의 세트" 처럼 UI 에 그대로 뜨는 이름. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Set")
	FText DisplayName;
};

/**
 * DT_ItemSetBonus 의 행. "몇 개를 착용하면 무엇을 얻는가" 한 줄이다.
 *
 * 효과는 **누적**된다. 4개를 착용하면 2단계와 4단계가 모두 적용되므로,
 * 각 단계에는 그 단계에서 **추가로 얻는 것만** 적는다.
 * 4단계 행에 2단계 내용을 다시 쓰면 두 번 적용된다.
 *
 *   RowName          SetId     RequiredCount  StatTag                       Op         Value
 *   FireSet_2_Dmg    FireSet   2              Stat.Offense.Damage.Magical   Increased  0.10
 *   FireSet_4_Dmg    FireSet   4              Stat.Offense.Damage.Magical   Increased  0.25
 *   FireSet_4_Crit   FireSet   4              Stat.Offense.CritChance       Added      0.05
 *
 * 장신구가 6칸이므로 단계는 2 / 4 / 6 정도가 자연스럽다.
 */
USTRUCT(BlueprintType)
struct FTDItemSetBonusRow : public FTableRowBase
{
	GENERATED_BODY()

	/** DT_ItemSet 의 RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Set")
	FName SetId;

	/** 이 효과가 켜지는 최소 착용 개수. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Set", meta = (ClampMin = "1"))
	int32 RequiredCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Set")
	FGameplayTag StatTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Set")
	ETDModOp Op = ETDModOp::Added;

	/** Increased/More 는 비율값. 10% 증가는 0.1 로 넣는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Set")
	float Value = 0.f;
};
