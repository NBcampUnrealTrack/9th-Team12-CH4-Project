#pragma once

#include "CoreMinimal.h"
#include "Shop/TDShopView.h"
#include "UI/Layer/WindowLayer/Common/WindowBase/TDWindowBaseWidget.h"
#include "TDShopWindowWidget.generated.h"

class UButton;
class UTextBlock;
class UTileView;
class UWidgetSwitcher;
class UTDInventorySlotListItem;
class UTDShopServiceComponent;

/**
 * WBP_ShopWindow의 C++ 부모입니다.
 *
 * C++은 상점 데이터와 버튼 동작만 담당합니다. 화면의 크기, 배치, 이미지와
 * 버튼 모양은 전부 WBP_ShopWindow가 소유하며 다른 창 위젯을 끌어오지 않습니다.
 */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDShopWindowWidget : public UTDWindowBaseWidget
{
	GENERATED_BODY()

public:
	void InitializeService(UTDShopServiceComponent* InService);
	void ApplyView(const FTDShopWindowView& InView);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	/** 장바구니와 판매 목록에는 최대 네 종류만 담을 수 있습니다. */
	static constexpr int32 SelectionSlotCount = 4;

	void RefreshWindow();
	void RefreshBuyItems();
	void RefreshBuyCart();
	void RefreshSellItems();
	void RefreshSellList();
	void RefreshButtons();
	void RefreshTabVisuals();
	void SanitizeSellSelection();

	const FTDShopBuyItemView* FindBuyItem(FName ItemId) const;
	const FTDShopSellItemView* FindSellItem(int32 InventorySlot) const;
	int64 GetBuyTotal() const;
	int64 GetSellTotal() const;
	int32 GetBuySelectionKindCount() const;
	int32 GetSellSelectionKindCount() const;
	int32 GetMaximumTradeCount() const;
	void SetStatusMessage(const FString& Message);

	UTDInventorySlotListItem* MakeBlankSlot(int32 SlotIndex);
	UTDInventorySlotListItem* MakeBuySlot(
		int32 SlotIndex,
		const FTDShopBuyItemView& Source,
		int32 Count,
		bool bCartSlot);
	UTDInventorySlotListItem* MakeSellSlot(
		const FTDShopSellItemView& Source,
		int32 Count,
		bool bSelectedSlot);
	void SetTileItems(
		UTileView* TileView,
		TArray<TObjectPtr<UTDInventorySlotListItem>>& Storage,
		TArray<UTDInventorySlotListItem*>& Items);

	void HandleBuyItemClicked(UObject* ItemObject);
	void HandleBuyCartItemClicked(UObject* ItemObject);
	void HandleSellItemClicked(UObject* ItemObject);
	void HandleSellListItemClicked(UObject* ItemObject);

	UFUNCTION()
	void HandleBuyTabClicked();

	UFUNCTION()
	void HandleSellTabClicked();

	UFUNCTION()
	void HandleBuyClearClicked();

	UFUNCTION()
	void HandleBuyClicked();

	UFUNCTION()
	void HandleSellClearClicked();

	UFUNCTION()
	void HandleSellClicked();

	TWeakObjectPtr<UTDShopServiceComponent> Service;

	UPROPERTY(Transient)
	FTDShopWindowView CurrentView;

	/** 구매/판매 두 페이지. 0=구매, 1=판매입니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetSwitcher> WS_ShopPages;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> BTN_BuyTab;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> BTN_SellTab;

	/** 상단의 현재 보유 골드. 구매/판매 페이지가 함께 사용합니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TXT_Gold;

	/** DT에 등록된 실제 상품만 표시하고 영역을 넘으면 스크롤합니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTileView> TV_BuyItems;

	/** 항상 네 칸이며, 일반 아이템은 종류별·장신구는 개체별로 담습니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTileView> TV_BuyCart;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TXT_BuyTotal;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> BTN_Buy;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> BTN_BuyClear;

	/** 판매 가능한 가방 아이템 목록. 퀘스트 아이템도 보이지만 비활성화됩니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTileView> TV_SellItems;

	/** 항상 네 칸입니다. 스택 아이템만 합치고 장신구는 각각 표시합니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTileView> TV_SellList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TXT_SellTotal;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> BTN_Sell;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> BTN_SellClear;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TXT_Result;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTDInventorySlotListItem>> BuyItemObjects;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTDInventorySlotListItem>> BuyCartObjects;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTDInventorySlotListItem>> SellItemObjects;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTDInventorySlotListItem>> SellListObjects;

	TMap<FName, int32> BuyCartCounts;
	TMap<int32, int32> SellCountsBySlot;

	int32 ActivePageIndex = 0;
	int32 RequestCounter = 0;
	int32 PendingRequestId = 0;
};
