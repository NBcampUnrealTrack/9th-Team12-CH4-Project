#pragma once

#include "CoreMinimal.h"
#include "Data/TDEnhanceRow.h"
#include "TDEnhanceStatics.generated.h"

/** 강화 시도 한 번의 결과. */
UENUM(BlueprintType)
enum class ETDEnhanceOutcome : uint8
{
	Success			UMETA(DisplayName = "성공"),
	Downgraded		UMETA(DisplayName = "하락"),

	/** 실패했지만 하락으로 이어지지 않았다. 레벨은 그대로다. */
	FailedNoChange	UMETA(DisplayName = "실패(변화 없음)")
};

/** 강화 시도 결과. 굴린 뒤의 사실만 담는다 — 골드 차감이나 복제 갱신은 호출한 쪽 몫이다. */
USTRUCT(BlueprintType)
struct FTDEnhanceRollResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TD|Enhance")
	ETDEnhanceOutcome Outcome = ETDEnhanceOutcome::FailedNoChange;

	/** Outcome 이 Downgraded 일 때만 의미 있다. 실제로 내려간 단계 수. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Enhance")
	int32 DowngradeTiers = 0;
};

/**
 * 강화 계산. 액터도 월드도 컴포넌트도 필요 없는 순수 함수로 둔다(D2 와 같은 이유) —
 * 확률표가 맞는지 월드를 띄우지 않고 단독으로 검증할 수 있어야 한다.
 *
 * 주사위 값을 **밖에서 넣어준다.** 함수 안에서 직접 굴리면 "3강에서 실패가 나오는
 * 주사위 값" 을 테스트 코드가 만들어낼 방법이 없다. 실제 굴리기는
 * UTDInventoryComponent::ServerEnhanceItem 이 한다(TDCombatStatics 의 CritRoll 과 같은 패턴).
 */
namespace TDEnhance
{
	/**
	 * 강화 한 번을 판정한다.
	 *
	 * @param Row           목표 레벨의 DT_Enhance 행.
	 * @param SuccessRoll   0~1. 이 값이 Row.SuccessRate 보다 작으면 성공.
	 * @param DowngradeRoll 0~1. 실패했을 때만 쓰인다 — 이 값이 DowngradeChanceOnFail 보다
	 *                      작으면 하락. 성공 판정에는 관여하지 않는다.
	 * @param TierRoll      0~1. 하락이 확정됐을 때 몇 단계 내려가는지 정한다.
	 *                      Min==Max 면(7~12강) 그 값 그대로, 아니면 그 사이를 선형으로 고른다.
	 */
	FTDEnhanceRollResult Roll(const FTDEnhanceRow& Row,
		float SuccessRoll, float DowngradeRoll, float TierRoll);

	/**
	 * 아이템의 착용 레벨제한에 따른 강화 1단당 스탯 증가율(0~1).
	 *
	 * 제일 낮은 레벨제한(0) 아이템은 1강당 2%, 제일 높은(캐릭터 최대 레벨) 아이템은
	 * 1강당 7% 로 선형 보간된다. 레벨제한이 낮은 초반 장비가 강화 효율까지 낮으면
	 * 이중으로 손해라, 완만한 곡선으로 격차를 좁혔다.
	 */
	float GetStatPercentPerLevel(int32 ItemRequiredLevel);

	/**
	 * 최종 배율. 장착 시 이 아이템의 고정 스탯 값에 그대로 곱한다.
	 * EnhanceLevel 이 0 이면 1.0(변화 없음)이다.
	 */
	float GetStatMultiplier(int32 EnhanceLevel, int32 ItemRequiredLevel);
}
