#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TDShopView.generated.h"

class UTexture2D;

/** 장바구니 구매 요청 한 줄. 같은 ItemId가 여러 번 오면 서버에서 합산한다. */
USTRUCT(BlueprintType)
struct FTDShopCartLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "TD|Shop")
	FName ItemId;

	UPROPERTY(BlueprintReadWrite, Category = "TD|Shop")
	int32 Count = 0;
};

/** 판매 목록에서 가방 한 칸의 아이템을 몇 개 팔지 나타냅니다. */
USTRUCT(BlueprintType)
struct FTDShopSellLine
{
	GENERATED_BODY()

	/** 서버가 다시 확인할 실제 인벤토리 슬롯 번호입니다. */
	UPROPERTY(BlueprintReadWrite, Category = "TD|Shop")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "TD|Shop")
	int32 Count = 0;
};

/** 상점이 판매하는 아이템 한 줄의 화면용 정보. */
USTRUCT(BlueprintType)
struct FTDShopBuyItemView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	TSoftObjectPtr<UTexture2D> Icon;

	/** WBP_SlotBase가 인벤토리와 같은 희귀도 테두리를 표시할 때 사용한다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	FGameplayTag Rarity;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	FGameplayTag ItemType;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	int32 Price = 0;

	/** false인 장신구 같은 아이템은 한 개가 장바구니 한 칸을 차지합니다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	bool bStackable = false;
};

/** 플레이어 가방에서 상점에 팔 수 있는 아이템 한 줄의 화면용 정보. */
USTRUCT(BlueprintType)
struct FTDShopSellItemView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	TSoftObjectPtr<UTexture2D> Icon;

	/** WBP_SlotBase가 인벤토리와 같은 희귀도 테두리를 표시할 때 사용한다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	FGameplayTag Rarity;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	FGameplayTag ItemType;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	int32 Count = 0;

	/** 판매할 실제 아이템의 강화 단계입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	int32 EnhanceLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	int32 UnitSellPrice = 0;

	/** 퀘스트 아이템처럼 bCanDiscard=false인 아이템은 false다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	bool bCanSell = false;

	/** true인 일반 묶음 아이템만 같은 ItemId끼리 판매 목록에서 합칩니다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	bool bStackable = false;
};

/** 서버가 상점창 한 개를 그리는 데 필요한 전체 스냅샷. */
USTRUCT(BlueprintType)
struct FTDShopWindowView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	int32 SessionId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	FName ShopId;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	FText ShopName;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	int32 Gold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	int64 InventoryRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	TArray<FTDShopBuyItemView> BuyItems;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	TArray<FTDShopSellItemView> SellItems;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	FString Message;

	/** 이 번호와 같은 요청을 기다리던 클라이언트 버튼의 잠금을 푼다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	int32 ReplyRequestId = 0;

	/** 장바구니 구매가 성공했을 때만 true다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	bool bClearCart = false;

	/** 선택한 판매 목록의 일괄 판매가 성공했을 때만 true입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Shop")
	bool bClearSellList = false;
};
