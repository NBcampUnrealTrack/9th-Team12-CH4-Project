#pragma once

#include "CoreMinimal.h"
#include "Data/TDShopRow.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Shop/TDShopTypes.h"
#include "TDShopStatics.generated.h"

class ATDPlayerState;
class UTDInventoryComponent;

/**
 * NPC 상점의 거래 진입점.
 *
 * 거래소(UTDMarketSubsystem)와 달리 서브시스템이 아니다. 재고가 무제한이고 가격이
 * 데이터라 **서버가 기억할 상태가 하나도 없기 때문이다.** 상태가 없으면 저장할 것도,
 * 맵을 넘겨 살려둘 것도 없으므로 함수 묶음으로 충분하다.
 *
 * 조회(GetShopEntries)는 클라이언트에서도 부를 수 있다. 테이블은 모든 머신이 갖고
 * 있으므로 목록을 그리려고 RPC 를 왕복할 이유가 없다. **거래(BuyItem/SellItem)만
 * 서버 전용**이며, 클라이언트는 ATDPlayerController 의 Server RPC 를 통로로 쓴다.
 */
UCLASS()
class TD_PROJECT_API UTDShopStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ── 조회 (어느 머신에서든) ────────────────────────────

	/** DT_Shop 조회. 창 제목에 쓴다. @return 행을 찾았으면 true. */
	UFUNCTION(BlueprintPure, Category = "TD|Shop")
	static bool GetShopInfo(FName ShopId, FTDShopRow& OutRow);

	/**
	 * 그 상점이 파는 목록. 되사는 값도 함께 담겨 온다.
	 *
	 * 순서는 DT_ShopItem 의 행 순서 그대로다. 진열 순서를 시트에서 정할 수 있게
	 * 정렬하지 않는다.
	 *
	 * @param Inventory  되팔기 값을 구하려고 받는다 — 아이템 정의(DT_ItemDefinition)의
	 *                   주인이 인벤토리 컴포넌트이기 때문이다. 비우면 구매가만 채워지고
	 *                   SellBackPrice 는 0 으로 온다. 목록 자체는 정상이다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Shop")
	static TArray<FTDShopEntry> GetShopEntries(const UTDInventoryComponent* Inventory, FName ShopId);

	/**
	 * 이 상점이 그 아이템을 되살 때 쳐주는 낱개 값.
	 *
	 * `DT_ItemDefinition.SellPrice × 상점 비율` 이다. **상점이 파는 물건인지는 보지 않는다** —
	 * 몬스터 드롭을 팔 수 있어야 하기 때문이다.
	 *
	 * 0 이 나와도 팔 수는 있다(0 원). 인벤토리 툴팁에 "판매가" 를 띄울 때 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Shop")
	static int32 GetSellBackPrice(const UTDInventoryComponent* Inventory, FName ShopId, FName ItemId);

	// ── 거래 (서버 전용) ──────────────────────────────────

	/**
	 * 산다. 골드를 치르고 아이템을 넣는다.
	 *
	 * 가방에 다 들어가지 않으면 **하나도 사지 않는다.** 일부만 넣으면 남은 수량의
	 * 골드를 어떻게 돌려줄지 다시 결정해야 한다.
	 *
	 * @param OutTotalPrice  실제로 치른 금액. 실패하면 0.
	 */
	static ETDShopResult BuyItem(ATDPlayerState* Buyer, FName ShopId, FName ItemId,
		int32 Count, int32& OutTotalPrice);

	/**
	 * 판다. 아이템을 덜어내고 골드를 준다.
	 *
	 * **상점 목록과 무관하다.** 값은 아이템 자신이 들고 있으므로(DT_ItemDefinition.SellPrice)
	 * 몬스터 드롭이든 보스 드롭이든 팔 수 있다. 못 파는 것은 bCanDiscard 가 false 인
	 * 물건뿐이다 — 버릴 수 없는 것은 넘길 수도 없다(D90).
	 *
	 * SellPrice 가 0 이면 0 원에 팔린다. 거절하지 않는 이유는 잡템을 비우는 통로가
	 * 필요해서다.
	 *
	 * @param OutTotalPrice  실제로 받은 금액. 실패하면 0.
	 */
	static ETDShopResult SellItem(ATDPlayerState* Seller, FName ShopId, int32 InventorySlot,
		int32 Count, int32& OutTotalPrice);

private:
	/** DT_ShopItem 에서 (상점, 아이템) 한 쌍을 찾는다. 없으면 nullptr. */
	static const struct FTDShopItemRow* FindShopItem(FName ShopId, FName ItemId);

	/** 그 상점의 되팔기 비율. 상점별 값이 있으면 그쪽이, 없으면 프로젝트 세팅이 이긴다. */
	static float GetSellBackRate(FName ShopId);

	/**
	 * 그 상점의 상인이 사거리 안에 있는가. **서버 전용 검증이다.**
	 *
	 * 없으면 어디서든 상점을 쓸 수 있고, 마을에 돌아갈 이유가 사라진다.
	 */
	static bool IsShopInRange(const ATDPlayerState* Player, FName ShopId);
};
