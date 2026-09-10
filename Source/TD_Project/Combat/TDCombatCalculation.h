#pragma once

#include "CoreMinimal.h"

/**
 * 데미지 최종값 계산.
 * TDStatCalculation 과 같은 이유로 순수 함수다 — 월드 없이 검증할 수 있어야 한다.
 */
struct FTDDamageInput
{
	float AttackDamage = 0.f;
	float CritChance = 0.f;            // 0~1
	float CritDamage = 1.5f;           // 총 배율. 1.5 = 150%
	float ArmorPenetration = 0.f;
	float TargetArmor = 0.f;
	float TargetDamageReduction = 0.f; // DT_StatDefinition 상한 클램프 전제

	/**
	 * 보스에게 주는 추가 피해. 0.2 면 20% 더 들어간다.
	 *
	 * **대상이 보스가 아니면 호출자가 0 을 넣는다.** 여기서 판단하지 않는 이유는
	 * "보스인가" 가 몬스터 테이블을 읽어야 알 수 있는 것이라, 그것까지 하면
	 * 이 함수가 더 이상 순수하지 않게 되기 때문이다.
	 */
	float BossDamageBonus = 0.f;
};

struct FTDDamageResult
{
	float FinalDamage = 0.f;
	bool bCritical = false;
};

namespace TDCombat
{
	/**
	 * @param CritRoll  [0,1) 난수. 호출자가 주입한다 — 테스트에서 크리를 강제하기 위해.
	 */
	FTDDamageResult CalculateDamage(const FTDDamageInput& Input, float CritRoll);
}