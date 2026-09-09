#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TDShopRow.generated.h"

/**
 * DT_Shop 의 행. 상점 하나의 표시 정보다.
 *
 * 파는 물건은 여기 없다. 상점 하나가 여러 아이템을 파는데 행 안에 배열을 넣으면
 * CSV 로 편집할 수 없게 되므로 DT_ShopItem 으로 나눈다 —
 * DT_ItemDefinition ↔ DT_ItemStat 과 같은 이유다(§5).
 *
 * 식별자는 RowName 이다. UTDShopComponent.ShopId 와 DT_ShopItem.ShopId 가
 * 정확히 같은 문자열이어야 한다("Shop_Forest" / "Shop_Desert").
 */
USTRUCT(BlueprintType)
struct FTDShopRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 상점 창 제목에 쓴다. 상인 이름과는 별개다 — 상인 이름은 DT_NPCDefinition 에 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FText DisplayName;

	/**
	 * 되팔 때 구매가의 몇 배를 쳐주는가. 0.25 면 100원짜리를 25원에 산다.
	 *
	 * 상점마다 둔 이유는 "비싸게 쳐주는 상인" 을 만들 여지를 남기기 위해서다.
	 * 전부 같은 값이면 UTDShopSettings 의 기본값을 그대로 쓰면 되도록,
	 * 0 이하로 두면 프로젝트 세팅 값이 쓰인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0"))
	float SellBackRateOverride = 0.f;
};
