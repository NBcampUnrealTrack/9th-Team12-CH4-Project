#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Stats/TDStatTypes.h"
#include "TDSkillPassiveRow.generated.h"

/**
 * DT_SkillPassive 의 행. 패시브 스킬이 올려주는 스탯 하나다.
 *
 * DT_ItemStat 과 열 구성이 거의 같다. 패시브에는 발동이라는 것이 없고 "찍어 두면
 * 항상 적용되는 스탯" 이 전부이므로, 장비가 주는 옵션과 완전히 같은 물건이다.
 * 읽어서 FTDStatModifier 배열로 만든 뒤 Source.Skill 로 등록하면 끝이다 —
 * 스킬을 위한 새 스탯 경로는 필요 없다.
 *
 *   RowName              SkillId        StatTag                    Op         BaseValue  ValuePerLevel
 *   Warrior_Tough_Armor  Warrior_Tough  Stat.Defense.Armor         Added      10         5
 *   Warrior_Tough_HP     Warrior_Tough  Stat.Resource.Health.Max   Increased  0.03       0.02
 *                        └ 같은 패시브, 스탯 둘
 */
USTRUCT(BlueprintType)
struct FTDSkillPassiveRow : public FTableRowBase
{
	GENERATED_BODY()

	/** DT_Skill 의 RowName. SkillType 이 Passive 인 행이어야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Passive")
	FName SkillId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Passive")
	FGameplayTag StatTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Passive")
	ETDModOp Op = ETDModOp::Added;

	/** 스킬 레벨 1 에서의 값. Increased/More 는 비율이라 3% 증가는 0.03 으로 넣는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Passive")
	float BaseValue = 0.f;

	/** 레벨 한 칸당 증가분. 레벨 L 의 값 = BaseValue + ValuePerLevel × (L - 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Passive")
	float ValuePerLevel = 0.f;
};
