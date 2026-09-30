#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TDDropTableRow.generated.h"

/**
 * DT_DropTable 의 행. "어느 드롭 목록에서 무엇이 몇 퍼센트로 나오는가" 한 줄이다.
 *
 * FTDMonsterRow 안에 배열로 넣으면 중첩 배열이 되어 CSV 로 편집할 수 없다.
 * 그래서 테이블을 나누고 DropTableId 로만 가리킨다(DT_ItemStat 과 같은 방식).
 * 여러 몬스터가 같은 목록을 함께 쓸 수 있다는 이점도 따라온다.
 *
 * 각 행은 **독립으로** 굴린다. 한 번 처치에 여러 줄이 동시에 터질 수 있고,
 * 아무것도 안 나오는 것이 보통이다. 가중치 추첨(반드시 하나는 나온다)으로 두면
 * 잡템 한 줄을 끼워 넣는 것만으로 다른 모든 줄의 확률이 함께 흔들려서,
 * "반지 확률만 조금 올리자" 가 불가능해진다.
 *
 *   RowName          DropTableId   ItemId        Chance  MinCount  MaxCount
 *   Slime_Potion     Drop_Slime    HPotion_Low   0.15    1         2
 *   Slime_FireRing   Drop_Slime    FireRing_Low  0.02    1         1
 */
USTRUCT(BlueprintType)
struct FTDDropTableRow : public FTableRowBase
{
	GENERATED_BODY()

	/** FTDMonsterRow::DropTableId 와 짝. 한 목록에 여러 행이 속한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop")
	FName DropTableId;

	/** DT_ItemDefinition 의 RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop")
	FName ItemId;

	/** 나올 확률. 0.02 면 2% 다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Chance = 0.f;

	/**
	 * 나올 때의 개수 범위.
	 *
	 * 장신구처럼 겹치지 않는 아이템은 둘 다 1 이어야 한다. 2 로 두면 한 번에 두 개가
	 * 들어와 인벤토리 칸을 두 개 먹는다 — 검증 스크립트가 이것을 잡는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "1"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drop", meta = (ClampMin = "1"))
	int32 MaxCount = 1;
};
