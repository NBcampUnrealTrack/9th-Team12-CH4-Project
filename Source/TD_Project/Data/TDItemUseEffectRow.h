#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "TDItemUseEffectRow.generated.h"

/**
 * DT_ItemUseEffect 의 행. 소비 아이템을 썼을 때 일어나는 일 하나다.
 *
 * 장비의 지속 효과와는 다른 테이블이다. 장비는 착용하는 동안 스탯을 바꾸므로
 * 모디파이어(DT_ItemStat)로 표현되지만, 소비는 쓰는 순간 한 번 일어나는 행동이다.
 *
 * 한 아이템이 여러 효과를 가질 수 있어 행을 나눈다. 행 안에 배열을 넣으면
 * CSV 로 편집할 수 없게 된다.
 *
 *   RowName        ItemId          EffectTag                     Value
 *   Elixir_HP      Elixir          Item.Effect.RestoreHealth     100
 *   Elixir_MP      Elixir          Item.Effect.RestoreMana        50
 *   Expand_Small   InvExpand       Item.Effect.ExpandInventory     5
 */
USTRUCT(BlueprintType)
struct FTDItemUseEffectRow : public FTableRowBase
{
	GENERATED_BODY()

	/** DT_ItemDefinition 의 RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Use")
	FName ItemId;

	/** Item.Effect.* — 무엇을 할지. 실제 동작은 UTDItemUseComponent 가 태그를 보고 정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Use")
	FGameplayTag EffectTag;

	/** 효과의 크기. 회복량이든 늘릴 칸 수든 태그에 따라 해석이 달라진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Use")
	float Value = 0.f;
};
