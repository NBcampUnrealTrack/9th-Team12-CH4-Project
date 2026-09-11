#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Skill/TDSkillTypes.h"
#include "Stats/TDStatTypes.h"
#include "TDSkillEffectRow.generated.h"

/**
 * DT_SkillEffect 의 행. 액티브 스킬을 썼을 때 대상에게 일어나는 일 하나다.
 *
 * DT_ItemUseEffect 와 같은 구조다. 한 스킬이 효과를 여럿 가질 수 있어 행을 나눈다 —
 * 행 안에 배열을 넣으면 CSV 로 편집할 수 없게 된다.
 *
 *   RowName            SkillId         EffectTag             BaseValue  ValuePerLevel
 *   Warrior_Slash_Dmg  Warrior_Slash   Skill.Effect.Damage   1.5        0.2
 *   Mage_Heal_Heal     Mage_Heal       Skill.Effect.Heal     50         15
 *   Mage_Heal_Dmg      Mage_Heal       Skill.Effect.Damage   0.8        0.1
 *                      └ 같은 스킬, 효과 둘
 *
 * 패시브는 이 테이블을 쓰지 않는다. 그쪽은 스탯 모디파이어라 DT_SkillPassive 가 맡는다.
 */
USTRUCT(BlueprintType)
struct FTDSkillEffectRow : public FTableRowBase
{
	GENERATED_BODY()

	/** DT_Skill 의 RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Effect")
	FName SkillId;

	/** Skill.Effect.* — 무엇을 할지. 실제 동작은 태그를 보고 정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Effect")
	FGameplayTag EffectTag;

	/**
	 * 스킬 레벨 1 에서의 값. **태그에 따라 의미가 다르다.**
	 *
	 *   Skill.Effect.Damage        공격력 배율. 1.5 면 공격력의 150%
	 *   Skill.Effect.Heal          회복량 (절대값)
	 *   Skill.Effect.RestoreMana   회복량 (절대값)
	 *
	 * 피해를 절대값이 아니라 배율로 두는 이유는, `ApplyDamage` 가 이미 공격자 스탯을
	 * 스냅샷하기 때문이다. 배율이면 장비·강화·레벨 성장이 전부 자동으로 따라온다.
	 * 고정값으로 두면 60 레벨에 스킬이 평타보다 약해진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Effect")
	float BaseValue = 0.f;

	/**
	 * 레벨 한 칸당 증가분. 레벨 L 의 값 = BaseValue + ValuePerLevel × (L - 1).
	 *
	 * 커브 테이블(FScalableFloat) 대신 선형으로 둔 이유는 편집 때문이다 —
	 * 커브를 쓰면 CSV 에 `(Value=1.5,Curve=(CurveTable=...))` 를 손으로 써야 하고
	 * 커브 테이블도 따로 만들어야 한다. DT_ClassGrowth 가 이미 같은 방식이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Effect")
	float ValuePerLevel = 0.f;

	/**
	 * 이 효과가 누구에게 가는가. 스킬이 아니라 **효과마다** 정한다.
	 *
	 * 마법사의 장판처럼 한 시전으로 아군은 회복하고 적은 피해를 입는 스킬이 있고,
	 * 그때 회복량과 피해 배율이 서로 다른 값이어야 한다. 행을 둘로 나누면
	 * 각자 BaseValue 를 가지므로 저절로 해결된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Effect")
	ETDSkillTarget TargetTeam = ETDSkillTarget::Enemy;

	// ── 지속 효과 전용 ────────────────────────────────────
	// Damage·Heal·RestoreMana 는 아래 세 칸을 쓰지 않는다.

	/**
	 * 효과가 유지되는 시간(초). 0 이면 즉발이다.
	 *
	 * Skill.Effect.Buff 와 Skill.Effect.Invulnerable 만 쓴다. 레벨을 따라 늘지 않는데,
	 * 시간이 길어지는 것보다 수치가 커지는 편이 체감이 분명하기 때문이다.
	 * 레벨마다 지속시간이 달라야 하는 스킬이 생기면 그때 열을 하나 더 만든다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Effect|Duration", meta = (ClampMin = "0"))
	float Duration = 0.f;

	/**
	 * 올려 줄 스탯. **Skill.Effect.Buff 에서만** 쓴다.
	 *
	 * DT_SkillPassive 와 같은 구조다. 다른 점은 Duration 이 지나면 걷힌다는 것뿐이라,
	 * 등록 경로도 그쪽과 같은 스탯 소스를 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Effect|Duration")
	FGameplayTag StatTag;

	/** Skill.Effect.Buff 에서만 쓴다. Increased/More 는 비율이라 20% 증가는 0.2 다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Effect|Duration")
	ETDModOp Op = ETDModOp::Added;
};
