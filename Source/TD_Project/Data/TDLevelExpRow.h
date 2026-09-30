#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TDLevelExpRow.generated.h"

/**
 * DT_LevelExp 의 행. 레벨 하나에 도달하는 데 필요한 **누적** 경험치다.
 *
 * 구간별 요구량이 아니라 누적값을 담는 이유는 저장 데이터 때문이다.
 * 경험치만 저장하고 레벨은 여기서 계산하면, 곡선을 조정했을 때
 * 기존 캐릭터의 레벨도 새 기준으로 다시 계산된다(D12).
 *
 *   Level  RequiredTotalExp
 *   1      0        ← 시작
 *   2      100      ← 누적 100 이 되면 2레벨
 *   3      350      ← 누적 350 이 되면 3레벨 (구간 요구량은 250)
 *
 * 테이블의 마지막 행이 곧 최대 레벨이다. 만렙을 올리려면 행을 더 넣으면 된다.
 */
USTRUCT(BlueprintType)
struct FTDLevelExpRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	int32 Level = 1;

	/** 이 레벨이 되기 위해 필요한 누적 경험치. 1레벨은 0 이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	int32 RequiredTotalExp = 0;
};
