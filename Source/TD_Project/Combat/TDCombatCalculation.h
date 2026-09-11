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