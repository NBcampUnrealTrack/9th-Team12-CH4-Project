#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TDStatTypes.h"

/**
 * 스탯 최종값 계산.
 *
 * 액터도 월드도 컴포넌트도 필요 없는 순수 함수로 둔다.
 * 계산 규칙이 맞는지는 월드를 띄우지 않고 단독으로 검증할 수 있어야 하기 때문.
 */
namespace TDStat
{
	/**
	 * 이 모디파이어가 대상 스탯 계산에 포함되는가.
	 *
	 * 두 가지를 본다.
	 *  1) 태그 계층 — Stat.Offense.Damage 모디파이어는 Stat.Offense.Damage.Fire 계산에도 걸린다.
	 *                 "모든 피해 +10%" 같은 광역 옵션을 한 줄로 표현하기 위한 것.
	 *  2) 조건      — 조건부 모디파이어는 Context가 쿼리를 통과할 때만 포함된다.
	 */
	bool AppliesTo(const FTDStatModifier& Modifier, FGameplayTag TargetStat, const FGameplayTagContainer& Context);

	/**
	 * 최종값 = (BaseValue + ΣBase + ΣAdded) × (1 + ΣIncreased) × Π(1 + More)
	 *
	 * Increased는 전부 더해진 뒤 한 번만 곱해지고, More는 각각 따로 곱해진다.
	 * 같은 15%라도 열 개 쌓이면 Increased는 ×2.5, More는 ×4.05가 된다.
	 *
	 * @param TargetStat  계산할 스탯. 이 태그와 계층이 맞는 모디파이어만 걸린다.
	 * @param BaseValue   DT_StatDefinition의 기본값. Op::Base 모디파이어와 함께 합산된다.
	 * @param Context     조건 평가용 태그. 비워두면 무조건부 모디파이어만 계산한다 —
	 *                    캐릭터 시트에 캐시할 값을 구할 때가 그 경우다.
	 *                    스킬 시전처럼 상황이 정해진 시점에는 해당 태그를 넘겨 조건부까지 포함시킨다.
	 * @param MinValue    클램프 하한. 저항 상한(75%)처럼 넘으면 안 되는 스탯에 쓴다.
	 * @param MaxValue    클램프 상한.
	 */
	float Calculate(
		FGameplayTag TargetStat,
		float BaseValue,
		TArrayView<const FTDStatModifier> Modifiers,
		const FGameplayTagContainer& Context = FGameplayTagContainer::EmptyContainer,
		float MinValue = -TNumericLimits<float>::Max(),
		float MaxValue = TNumericLimits<float>::Max());
}
