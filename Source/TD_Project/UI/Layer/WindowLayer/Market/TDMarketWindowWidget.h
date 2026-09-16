#pragma once

#include "CoreMinimal.h"
#include "Market/TDMarketTypes.h"
#include "UI/Layer/WindowLayer/Common/WindowBase/TDWindowBaseWidget.h"
#include "TDMarketWindowWidget.generated.h"

class UButton;
class UEditableTextBox;
class UScrollBox;
class UTextBlock;
class UWidgetSwitcher;
class ATDPlayerController;
class UTDMarketListingEntryWidget;

/** 지금 보고 있는 탭. */
UENUM()
enum class ETDMarketTab : uint8
{
	/** 남이 올린 매물을 찾아 산다. */
	Buy,

	/** 내가 올린 매물을 보고 내린다. 등록도 여기서 한다. */
	Mine
};

/**
 * 거래소 창.
 *
 * 서버 처리는 UTDMarketSubsystem 이 모두 끝내 두었고, 이 창은 ATDPlayerController 의
 * 통로(ServerSearchListings 등)를 부르고 결과 델리게이트를 받아 그리기만 한다.
 * 강화·추가 옵션과 달리 NPC 세션이 없어 서비스 컴포넌트도 없다.
 *
 * ── 목록은 스냅샷이다 ──
 * 매물 전체를 복제하지 않는다. 검색한 그 시점의 목록을 받을 뿐이라, 보는 사이에
 * 남이 사 갈 수 있다. 그때 구매는 ListingNotFound 로 거부되는데 **오류가 아니라
 * 흔한 일이다.** 그 결과를 받으면 말없이 목록을 새로 고친다.
 */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDMarketWindowWidget : public UTDWindowBaseWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 목록 한 줄로 쓸 WBP. 지정하지 않으면 목록이 비어 보인다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Market")
	TSubclassOf<UTDMarketListingEntryWidget> ListingEntryClass;

	/** 구매 탭과 내 판매 탭을 오가는 스위처. 0 = 구매, 1 = 내 판매. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> WS_Tabs;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_TabBuy;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_TabMine;

	// ── 구매 탭 ───────────────────────────────────────────

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> SB_Listings;

	/** 아이템 ID 로 거른다. 비우면 전부. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> ETB_SearchFilter;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_Search;

	/** 목록이 비었을 때만 보이는 안내. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_ListingsEmpty;

	// ── 내 판매 탭 ────────────────────────────────────────

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> SB_MyListings;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_MyListingsEmpty;

	/** 등록할 인벤토리 칸 번호. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> ETB_ListSlot;

	/** 등록할 가격. 묶음 전체의 값이다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> ETB_ListPrice;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_ListItem;

	// ── 공통 ──────────────────────────────────────────────

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Gold;

	/** 결과 문구. 성공도 실패도 여기 뜬다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Result;

private:
	ATDPlayerController* GetMarketController() const;

	void ShowTab(ETDMarketTab Tab);

	/** 지금 탭에 맞는 목록을 서버에 다시 요청한다. */
	void RefreshCurrentTab();

	void RefreshGold();

	void RebuildList(UScrollBox* Target, const TArray<FTDMarketListing>& Listings,
		bool bOwnListings, UTextBlock* EmptyLabel);

	UFUNCTION()
	void HandleTabBuyClicked();

	UFUNCTION()
	void HandleTabMineClicked();

	UFUNCTION()
	void HandleSearchClicked();

	UFUNCTION()
	void HandleListItemClicked();

	/** 목록의 버튼. 구매 탭이면 사고, 내 판매 탭이면 내린다. */
	UFUNCTION()
	void HandleEntrySelected(int32 ListingId);

	UFUNCTION()
	void HandleMarketResult(ETDMarketResult Result, int32 ListingId);

	UFUNCTION()
	void HandleSearchResult(const TArray<FTDMarketListing>& Listings);

	ETDMarketTab CurrentTab = ETDMarketTab::Buy;

	/** 내 이름. 구매 탭에서 자기 매물을 가려낼 때 쓴다. */
	FString MyName;
};
