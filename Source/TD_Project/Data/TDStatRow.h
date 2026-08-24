#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "TDStatRow.generated.h"

/**
 * DT_StatDefinition 의 행. 스탯 하나의 메타데이터를 담는다.
 *
 * 스탯의 "값"이 아니라 "정의"다. 실제 수치는 UTDStatComponent 가 계산하고,
 * 이 테이블은 기본값과 허용 범위, UI 표시 방법을 알려준다.
 *
 * 레벨당 성장량은 여기 없다. 직업마다 다르므로 DT_ClassGrowth 가 담당한다.
 */
USTRUCT(BlueprintType)
struct FTDStatRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 이 행이 정의하는 스탯. 예: Stat.Offense.Damage.Physical */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	FGameplayTag StatTag;

	/** UI 표시용 이름. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	FText DisplayName;

	/**
	 * 모디파이어가 하나도 없을 때의 값.
	 *
	 * 태그마다 독립이며 계층을 타지 않는다. 상위 태그인 Stat.Offense.Damage 에 값을 넣어도
	 * Physical/Magical 계산에는 들어가지 않으므로, 상위 태그 행의 기본값은 0 으로 둔다.
	 * 계층이 적용되는 것은 모디파이어뿐이다.
	 *
	 * 이동 속도처럼 0 이면 곤란한 스탯은 반드시 값을 채울 것 (CharacterMovement 기본값 600).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	float DefaultValue = 0.f;

	// ── 허용 범위 ─────────────────────────────────────────
	// 제한 여부를 값이 아니라 별도 플래그로 표현한다.
	// "0이면 무제한" 같은 관례를 쓰면 상한이 실제로 0인 스탯을 표현할 수 없고,
	// 플래그를 빠뜨렸을 때 모든 스탯이 0으로 잘리는 사고가 조용히 난다.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Range")
	bool bHasMinValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Range", meta = (EditCondition = "bHasMinValue"))
	float MinValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Range")
	bool bHasMaxValue = false;

	/**
	 * 넘으면 안 되는 값. 받는 피해 감소는 1.0 에서 무적이 되므로 0.75 정도로 막고,
	 * 치명타 확률은 1.0 을 넘어봐야 의미가 없다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Range", meta = (EditCondition = "bHasMaxValue"))
	float MaxValue = 0.f;

	// ── UI 표시 ───────────────────────────────────────────

	/** 참이면 0.2 를 "20%" 로 표시한다. 치명타 확률, 방어력 무시, 받는 피해 감소 등이 해당한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Display")
	bool bIsPercent = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat|Display")
	int32 DecimalPlaces = 0;
};
