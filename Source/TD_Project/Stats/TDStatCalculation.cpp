#include "TDStatCalculation.h"

namespace TDStat
{
	bool AppliesTo(const FTDStatModifier& Modifier, FGameplayTag TargetStat, const FGameplayTagContainer& Context)
	{
		// MatchesTag는 "TargetStat이 Modifier.Stat이거나 그 하위인가"를 본다.
		// Damage.Fire를 계산할 때 Damage 모디파이어는 걸리지만, 그 반대는 걸리지 않는다.
		if (!TargetStat.MatchesTag(Modifier.Stat))
		{
			return false;
		}

		// 조건이 없으면 항상 적용된다.
		if (!Modifier.IsConditional())
		{
			return true;
		}

		// 조건부인데 Context가 비어 있으면 제외된다.
		// 빈 컨테이너는 어떤 쿼리도 통과하지 못하므로 별도 분기 없이 처리된다.
		return Context.MatchesQuery(Modifier.Condition);
	}

	float Calculate(
		FGameplayTag TargetStat,
		float BaseValue,
		TArrayView<const FTDStatModifier> Modifiers,
		const FGameplayTagContainer& Context,
		float MinValue,
		float MaxValue)
	{
		float Flat = BaseValue;
		float SumIncreased = 0.f;
		float MoreMultiplier = 1.f;

		for (const FTDStatModifier& Modifier : Modifiers)
		{
			if (!AppliesTo(Modifier, TargetStat, Context))
			{
				continue;
			}

			switch (Modifier.Op)
			{
			// Base와 Added는 계산상 똑같이 합산된다. 타입을 나눠 둔 것은
			// 나중에 "무기 기본 공격력의 N%"처럼 원천값만 참조하는 옵션을 위해서다.
			case ETDModOp::Base:
			case ETDModOp::Added:
				Flat += Modifier.Value;
				break;

			case ETDModOp::Increased:
				SumIncreased += Modifier.Value;
				break;

			case ETDModOp::More:
				MoreMultiplier *= (1.f + Modifier.Value);
				break;
			}
		}

		const float Result = Flat * (1.f + SumIncreased) * MoreMultiplier;
		return FMath::Clamp(Result, MinValue, MaxValue);
	}
}
