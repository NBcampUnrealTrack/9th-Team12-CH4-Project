#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TDShopItemRow.generated.h"

/**
 * DT_ShopItem 의 행. 어느 상점이 무엇을 얼마에 파는가.
 *
 * **파는 목록이자 되사는 가격표다.** 여기 없는 물건은 그 상점이 사지도 않는다 —
 * 되팔 값을 매길 근거가 없기 때문이다. 몬스터가 떨군 장비를 0원에 넘기는 사고를
 * 막는 것도 겸한다.
 *
 * 재고 열이 없다. 무제한으로 팔기로 했다(재고를 두면 남은 수를 서버가 기억해야 해
 * 저장 대상이 생기고, 회복 주기도 정해야 한다).
 *
 *   RowName            ShopId       ItemId      Price
 *   Forest_Potion      Shop_Forest  HpPotion      50
 *   Forest_Expand      Shop_Forest  InvExpand   1000
 *   Desert_Potion      Shop_Desert  HpPotion      60   ← 같은 물건, 다른 값
 */
USTRUCT(BlueprintType)
struct FTDShopItemRow : public FTableRowBase
{
	GENERATED_BODY()

	/** DT_Shop 의 RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop Item")
	FName ShopId;

	/** DT_ItemDefinition 의 RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop Item")
	FName ItemId;

	/** 낱개 가격. 여러 개를 사면 곱해진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop Item", meta = (ClampMin = "0"))
	int32 Price = 0;
};
