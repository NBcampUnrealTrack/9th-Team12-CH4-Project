#pragma once

#include "CoreMinimal.h"
#include "Items/TDItemTypes.h"
#include "TDMarketTypes.generated.h"

/**
 * 거래소에 올라온 물건 하나.
 *
 * 아이템을 ItemId 가 아니라 FTDItemInstance 통째로 담는 것이 핵심이다. ID 만 두면
 * 11강 반지를 사도 0강짜리가 오고, 굴려진 추가 옵션도 전부 사라진다.
 *
 * 등록하면 아이템은 **판매자 인벤토리에서 빠진다.** 남겨 두면 등록해 놓고 그대로
 * 착용하거나 버릴 수 있어 복제가 된다.
 */
USTRUCT(BlueprintType)
struct FTDMarketListing
{
	GENERATED_BODY()

	/** 서버가 발급하는 고유 번호. 구매·취소는 이 번호로만 지목한다. */
	UPROPERTY(BlueprintReadOnly, Category = "Market")
	int32 ListingId = 0;

	/** 판매자 이름. 표시용이다. */
	UPROPERTY(BlueprintReadOnly, Category = "Market")
	FString SellerName;

	/** 판매 물건. 강화 단수와 추가 옵션까지 그대로 보존된다. */
	UPROPERTY(BlueprintReadOnly, Category = "Market")
	FTDItemInstance Item;

	/**
	 * 묶음 전체의 값. **낱개 가격이 아니다** — 부분 구매를 하지 않기 때문이다.
	 *
	 * 포션 20개를 1000골드에 올리면 20개를 한꺼번에 1000골드에 산다. 낱개로 나누면
	 * 수량·가격 계산에 더해 한 매물이 여러 번 팔리는 동안의 경합까지 다뤄야 한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Market")
	int32 Price = 0;
};

/**
 * 거래소 요청의 결과.
 *
 * 문구가 아니라 enum 인 이유는 ETDZoneTravelResult 와 같다 — UI 가 문구를 정한다.
 */
UENUM(BlueprintType)
enum class ETDMarketResult : uint8
{
	Success				UMETA(DisplayName = "성공"),

	/**
	 * 그 번호의 매물이 없다. **정상적으로 자주 일어난다** —
	 * 검색 결과는 그 시점의 스냅샷이라, 목록을 보는 사이에 남이 사 갈 수 있다.
	 * UI 는 이때 목록을 새로 고치면 된다.
	 */
	ListingNotFound		UMETA(DisplayName = "이미 팔렸거나 없는 매물"),

	NotEnoughGold		UMETA(DisplayName = "골드 부족"),

	/** 살 물건을 넣을 자리가 없다. 골드는 빠지지 않았다. */
	InventoryFull		UMETA(DisplayName = "가방 공간 부족"),

	/** 자기 물건은 살 수 없다. 취소를 쓰면 된다. */
	CannotBuyOwnListing	UMETA(DisplayName = "자기 매물"),

	/** 남의 매물은 취소할 수 없다. */
	NotSeller			UMETA(DisplayName = "판매자가 아님"),

	/** 등록하려는 슬롯이 비었거나 없다. */
	ItemNotFound		UMETA(DisplayName = "아이템 없음"),

	/** 가격이 0 이하이거나 상한을 넘었다. */
	InvalidPrice		UMETA(DisplayName = "가격 오류"),

	/** 한 사람이 올릴 수 있는 개수를 넘었다. */
	TooManyListings		UMETA(DisplayName = "등록 한도 초과"),

	/** 퀘스트 아이템처럼 거래할 수 없는 물건이다. */
	ItemNotTradable		UMETA(DisplayName = "거래 불가 아이템"),

	/**
	 * 아직 캐릭터를 고르지 않았다.
	 *
	 * 판매자를 캐릭터 이름으로 식별하므로, 고르기 전에는 이름이 "mpc-3B8A..." 같은
	 * 접속 ID 다. 그대로 두면 판매자를 다시 찾을 수 없어 대금을 줄 방법이 없어진다.
	 */
	NoCharacterSelected	UMETA(DisplayName = "캐릭터 미선택"),

	InternalError		UMETA(DisplayName = "내부 오류")
};
