#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Stats/TDStatTypes.h"
#include "TDUnionBonusRow.generated.h"

/**
 * DT_UnionBonus 의 행. "이 직업을 몇 레벨까지 키우면 계정의 모든 캐릭터가 무엇을 얻는가" 한 줄이다.
 *
 * 메이플 유니온과 같은 발상이다. 캐릭터 하나를 키우는 보상이 그 캐릭터에서 끝나지 않고
 * 계정 전체로 번져, 다른 직업을 키울 이유가 생긴다.
 *
 * 효과는 **누적**된다(DT_ItemSetBonus 와 같다). 50레벨이면 30·40·50 행이 모두 적용되므로
 * 각 행에는 그 단계에서 추가로 얻는 것만 적는다.
 *
 * 같은 직업을 여러 명 키워도 **가장 높은 레벨 하나만** 센다(TDUnion::BuildModifiers).
 * 지금 플레이 중인 캐릭터도 센다.
 *
 *   RowName      ClassId   RequiredLevel  StatTag                          Op         Value
 *   Warrior_30   Warrior   30             Stat.Resource.Health.Max         Increased  0.03
 *   Archer_30    Archer    30             Stat.Offense.CritChance          Added      0.02
 */
USTRUCT(BlueprintType)
struct FTDUnionBonusRow : public FTableRowBase
{
	GENERATED_BODY()

	/** DT_CharacterClass 의 RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Union")
	FName ClassId;

	/** 이 직업의 최고 레벨이 이 값 이상이면 켜진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Union", meta = (ClampMin = "1"))
	int32 RequiredLevel = 30;

	/**
	 * 모든 직업에 쓸모 있는 스탯을 고른다. 전사로 플레이하는데 마법사 유니온이 마법공격력을
	 * 주면 버려진다 — 그래서 체력·치명타 확률·쿨다운 회복처럼 공격 종류를 가리지 않는 것을 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Union")
	FGameplayTag StatTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Union")
	ETDModOp Op = ETDModOp::Added;

	/** Increased 는 비율값이다. 3% 는 0.03. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Union")
	float Value = 0.f;
};
