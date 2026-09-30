#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TDEnhanceRow.generated.h"

/**
 * DT_Enhance 의 행. **강화 레벨 하나로 가는 시도**를 정의한다.
 * "Level 7" 행은 6강에서 7강으로 올릴 때의 확률·비용이다.
 *
 * 확률은 아이템 종류와 무관하게 **레벨 하나로만 정해진다** — 같은 7강 시도는
 * 어떤 아이템이든 똑같이 작동한다. 아이템마다 다른 것은 성공했을 때 오르는
 * 스탯의 폭뿐이고, 그건 이 테이블이 아니라 TDEnhanceStatics 가
 * (강화 레벨, 아이템의 착용 레벨제한) 으로 계산한다 — 낮은 레벨 아이템은
 * 1강당 +2%, 높은 레벨 아이템은 1강당 +7% 로 완만하게 보간된다.
 *
 * ── 세 구간 ──
 *   1~6강    하락 없음. 성공률만 낮아진다
 *   7~12강   실패 시 하락이 생긴다(1단계 고정). 6강 끝에서 한 번 확률이 다시 오른 뒤
 *            다시 낮아진다 — 하락 위험이 생기는 대신 진입 문턱을 낮춘 것이다
 *   13강~    실패 시 하락이 랜덤 1~3단계. 그 대신 13강의 성공률·하락확률은
 *            **7강과 정확히 같다** — 사실상 7~12강 구간이 그대로 반복된다
 */
USTRUCT(BlueprintType)
struct FTDEnhanceRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 이 행이 나타내는 도달 레벨. RowName 은 "Lv07" 같은 편집용 별칭일 뿐 조회에 쓰지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enhance")
	int32 Level = 1;

	/** 이 레벨로 성공할 확률. 0~1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enhance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SuccessRate = 1.f;

	/**
	 * **실패했을 때** 그중 하락으로 이어질 확률. 0~1.
	 *
	 * 성공률과 곱하는 값이 아니다 — "실패라는 전제 아래" 의 조건부 확률이다.
	 * 1~6강은 0 이라 실패해도 그 자리에 머문다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enhance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DowngradeChanceOnFail = 0.f;

	/** 하락 시 내려가는 단계의 최소값. 7~12강은 1로 고정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enhance", meta = (ClampMin = "0"))
	int32 MinDowngradeTiers = 0;

	/** 하락 시 내려가는 단계의 최대값. 13강부터 3까지 벌어진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enhance", meta = (ClampMin = "0"))
	int32 MaxDowngradeTiers = 0;

	/** 시도 비용. 실패해도 차감된다 — 시도 자체의 대가이지 성공 보수가 아니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enhance", meta = (ClampMin = "0"))
	int32 Cost = 0;
};
