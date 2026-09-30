#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Market/TDMarketTypes.h"
#include "TDMarketListingEntryWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTDOnMarketEntrySelected,
	int32,
	ListingId);

/**
 * 거래소 목록 한 줄. 구매 탭과 내 판매 탭이 같은 줄을 쓴다.
 *
 * 두 탭의 차이는 버튼 글자뿐이라(사기 / 내리기) 위젯을 나누지 않았다.
 * 나누면 아이콘·이름·가격을 그리는 코드가 두 벌이 되고, 한쪽만 고쳐지는 일이 생긴다.
 */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDMarketListingEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 한 줄을 채운다.
	 *
	 * @param bOwnListing  내 매물인가. 구매 탭에서는 살 수 없게 막고,
	 *                     내 판매 탭에서는 버튼이 "내리기" 가 된다.
	 */
	void SetupEntry(const FTDMarketListing& InListing, bool bOwnListing);

	FTDOnMarketEntrySelected OnSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_Action;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IMG_ItemIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_ItemName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_SellerName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Price;

	/** 버튼 안의 글자. "구매" 와 "내리기" 를 오간다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_ActionLabel;

private:
	UFUNCTION()
	void HandleClicked();

	void RefreshEntry();

	UPROPERTY(Transient)
	FTDMarketListing Listing;

	bool bIsOwnListing = false;
};
