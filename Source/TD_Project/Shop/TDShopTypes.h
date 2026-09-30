#pragma once

#include "CoreMinimal.h"
#include "TDShopTypes.generated.h"

/**
 * 상점 거래의 결과.
 *
 * 문구가 아니라 코드를 돌려주는 이유는 거래소(ETDMarketResult)와 같다 — 서버가 완성된
 * 문장을 보내면 현지화도 못 하고 화면 디자인이 서버 코드에 묶인다.
 */
UENUM(BlueprintType)
enum class ETDShopResult : uint8
{
	Success					UMETA(DisplayName = "성공"),

	/** DT_Shop 에 그 ShopId 가 없다. 데이터 오타이거나 위조된 요청이다. */
	ShopNotFound			UMETA(DisplayName = "상점 없음"),

	/** 그 ShopId 를 가진 상인이 사거리 안에 없다. */
	TooFar					UMETA(DisplayName = "너무 멀다"),

	/**
	 * 이 상점이 팔지 않는 물건이다. **살 때만 나온다.**
	 *
	 * 되팔기는 상점 목록을 보지 않는다 — 값을 아이템 자신이 들고 있어서
	 * (DT_ItemDefinition.SellPrice) 몬스터 드롭도 팔 수 있다.
	 */
	ItemNotSold				UMETA(DisplayName = "취급하지 않음"),

	NotEnoughGold			UMETA(DisplayName = "골드 부족"),
	InventoryFull			UMETA(DisplayName = "가방 가득참"),

	/** 팔려는 칸이 비었거나 개수가 모자란다. */
	ItemNotFound			UMETA(DisplayName = "아이템 없음"),

	/** 퀘스트 아이템처럼 넘길 수 없는 물건이다. bCanDiscard 로 판단한다(D90). */
	ItemNotTradable			UMETA(DisplayName = "거래 불가"),

	/** 개수가 0 이하이거나 상한을 넘었다. */
	InvalidCount			UMETA(DisplayName = "개수 오류"),

	NoCharacterSelected		UMETA(DisplayName = "캐릭터 없음"),
	InternalError			UMETA(DisplayName = "내부 오류")
};

/**
 * 상점 목록의 한 줄. 서버가 클라이언트에 보내는 형태다.
 *
 * DT_ShopItem 행을 그대로 보내지 않는 이유는 ShopId 가 중복이기 때문이다 —
 * 어느 상점인지는 요청할 때 이미 정해져 있다.
 */
USTRUCT(BlueprintType)
struct FTDShopEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	FName ItemId;

	/** 낱개 가격. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	int32 Price = 0;

	/**
	 * 이 상점이 되사 줄 때의 낱개 값. `DT_ItemDefinition.SellPrice × 상점 비율` 이다.
	 *
	 * 화면에서 다시 곱하지 않게 미리 넣어 둔다. 반올림 방식이 어긋나면 표시값과
	 * 실제로 들어오는 골드가 1 씩 달라진다.
	 *
	 * 구매가(Price)와 무관한 값이라 0 일 수 있다 — 사기만 하고 되팔 수는 없는 물건이다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	int32 SellBackPrice = 0;
};

/**
 * 사거나 판 결과. **UI 가 구독할 지점이다.**
 *
 * 성공했을 때도 온다 — "물약 3개를 150골드에 샀습니다" 같은 안내를 띄울 자리다.
 * 목록은 다시 그릴 필요가 없다(재고가 무제한이라 변하지 않는다). 골드와 인벤토리는
 * 각자의 복제 알림으로 갱신된다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FTDOnShopResult,
	ETDShopResult, Result, FName, ItemId, int32, Count, int32, TotalPrice);
