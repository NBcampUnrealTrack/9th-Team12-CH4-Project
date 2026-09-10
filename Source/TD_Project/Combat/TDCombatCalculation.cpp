#include "TDCombatCalculation.h"

namespace TDCombat
{
	FTDDamageResult CalculateDamage(const FTDDamageInput& Input, float CritRoll)
	{
		FTDDamageResult Result;

		// 1. 크리 판정. CritRoll(0~1 난수)이 크리 확률보다 작으면 크리티컬.
		//    예: CritChance 0.3 이면 난수가 0.3 미만일 확률 = 30%
		const float CritChance = FMath::Clamp(Input.CritChance, 0.f, 1.f);
		Result.bCritical = CritRoll < CritChance;

		float Damage = Input.AttackDamage;
		if (Result.bCritical)
		{
			Damage *= Input.CritDamage;   // 크리면 1.5배 (기본값 기준)
		}

		// 2-1. 보스 추가 피해. 대상이 보스가 아니면 호출자가 0 을 넣으므로 1배가 된다.
		//      방어 계산 앞뒤 어디에 두어도 전부 곱셈이라 결과는 같다 — 크리 옆이 읽기 쉽다.
		Damage *= 1.f + FMath::Max(0.f, Input.BossDamageBonus);

		// 2. 유효 방어 = 방어력 - 방어무시. 0 밑으로는 못 내려간다.
		const float EffectiveArmor = FMath::Max(0.f, Input.TargetArmor - Input.ArmorPenetration);

		// 3. 방어 적용. 방어 100 이면 절반, 방어 300 이면 1/4 — 점감형이라 무한 스택에 안전.
		Damage *= 100.f / (100.f + EffectiveArmor);

		// 4. 받는 피해 감소. 0.3 이면 30% 덜 받는다.
		Damage *= 1.f - Input.TargetDamageReduction;

		// 5. 바닥값. "때렸는데 0" 방지. (§8 합의 전 임시 규칙)
		Result.FinalDamage = FMath::Max(1.f, Damage);

		return Result;
	}
}