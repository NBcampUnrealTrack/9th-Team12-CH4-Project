#include "Items/TDEnhanceStatics.h"

namespace TDEnhance
{
	FTDEnhanceRollResult Roll(const FTDEnhanceRow& Row,
		float SuccessRoll, float DowngradeRoll, float TierRoll)
	{
		FTDEnhanceRollResult Result;

		if (SuccessRoll < Row.SuccessRate)
		{
			Result.Outcome = ETDEnhanceOutcome::Success;
			return Result;
		}

		// 실패했다. 하락 확률이 0 이면(1~6강) 그 자리에 머문다.
		if (Row.DowngradeChanceOnFail <= 0.f || DowngradeRoll >= Row.DowngradeChanceOnFail)
		{
			Result.Outcome = ETDEnhanceOutcome::FailedNoChange;
			return Result;
		}

		Result.Outcome = ETDEnhanceOutcome::Downgraded;

		// Min == Max 면(7~12강) 그 값 그대로. 다르면(13강~) 그 사이를 TierRoll 로 고른다.
		const int32 TierRange = Row.MaxDowngradeTiers - Row.MinDowngradeTiers;
		Result.DowngradeTiers = Row.MinDowngradeTiers
			+ (TierRange > 0 ? FMath::Min(TierRange, FMath::FloorToInt(TierRoll * (TierRange + 1))) : 0);

		return Result;
	}

	float GetStatPercentPerLevel(int32 ItemRequiredLevel)
	{
		// 캐릭터 레벨 상한(DT_LevelExp 의 50)에 맞춘 기준값이다. 나중에 만렙이 바뀌면
		// 이 상수도 함께 조정해야 한다 — GetMaxLevel() 을 쓰지 않는 이유는 이 함수를
		// 순수하게 두기 위해서다(월드도 컴포넌트도 필요 없게).
		constexpr float ReferenceMaxLevel = 50.f;
		constexpr float MinPercent = 0.02f;
		constexpr float MaxPercent = 0.07f;

		const float Alpha = FMath::Clamp(
			static_cast<float>(ItemRequiredLevel) / ReferenceMaxLevel, 0.f, 1.f);

		return FMath::Lerp(MinPercent, MaxPercent, Alpha);
	}

	float GetStatMultiplier(int32 EnhanceLevel, int32 ItemRequiredLevel)
	{
		if (EnhanceLevel <= 0)
		{
			return 1.f;
		}

		return 1.f + EnhanceLevel * GetStatPercentPerLevel(ItemRequiredLevel);
	}
}
