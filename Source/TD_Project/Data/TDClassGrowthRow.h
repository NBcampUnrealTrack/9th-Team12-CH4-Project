#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "TDClassGrowthRow.generated.h"

/**
 * DT_ClassGrowth 의 행. 직업 하나가 레벨당 얼마나 성장하는지를 스탯별로 적는다.
 *
 * 스탯 포인트를 직접 찍지 않고 레벨업 시 자동으로 오르는 구조이므로,
 * "어떤 직업이 무엇을 얼마나 얻는가"가 곧 직업의 정체성이 된다.
 *
 * 한 직업이 여러 스탯을 성장시키므로 (ClassId, StatTag) 조합마다 한 행이다.
 * RowName 은 편집용 별칭일 뿐이며 조회에는 쓰지 않는다 — 중첩 배열을 피하려고
 * 테이블을 평평하게 두고 두 컬럼으로 필터링한다.
 *
 *   RowName              ClassId   StatTag                             ValuePerLevel
 *   Default_Health       Default   Stat.Resource.Health.Max            10
 *   Physical_Health      Physical  Stat.Resource.Health.Max            20
 *   Physical_Damage      Physical  Stat.Offense.Damage.Physical         5
 *   Magical_Mana         Magical   Stat.Resource.Mana.Max              20
 *   Magical_Damage       Magical   Stat.Offense.Damage.Magical          5
 */
USTRUCT(BlueprintType)
struct FTDClassGrowthRow : public FTableRowBase
{
	GENERATED_BODY()

	/**
	 * 직업 식별자. Default / Physical / Magical.
	 *
	 * Default 는 직업과 무관하게 모두가 얻는 성장이다. 직업 행과 함께 적용되므로
	 * 공통 부분을 직업마다 반복해서 쓰지 않아도 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class Growth")
	FName ClassId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class Growth")
	FGameplayTag StatTag;

	/**
	 * 레벨 1당 오르는 양.
	 *
	 * 이 값으로 계산된 결과는 저장하지 않는다. Level 과 ClassId 만 저장해 두면
	 * 접속할 때마다 다시 구해지고, 덕분에 성장률 밸런스를 고쳐도 기존 유저에게
	 * 바로 반영된다. 결과를 저장했다면 계정 전체를 훑는 마이그레이션이 필요하다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class Growth")
	float ValuePerLevel = 0.f;
};
