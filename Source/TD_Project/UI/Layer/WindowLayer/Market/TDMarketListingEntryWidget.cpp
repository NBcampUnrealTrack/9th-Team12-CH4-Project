#include "UI/Layer/WindowLayer/Market/TDMarketListingEntryWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Data/TDItemRow.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Items/TDInventoryComponent.h"
#include "Player/TDPlayerState.h"
#include "UI/Common/Tooltip/TDItemTooltipWidget.h"

#define LOCTEXT_NAMESPACE "TDMarketListingEntryWidget"

void UTDMarketListingEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BTN_Action)
	{
		BTN_Action->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
	}

	RefreshEntry();
}

void UTDMarketListingEntryWidget::NativeDestruct()
{
	if (BTN_Action)
	{
		BTN_Action->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
	}

	UTDItemTooltipWidget::ClearItemTooltip(this);

	Super::NativeDestruct();
}

void UTDMarketListingEntryWidget::SetupEntry(const FTDMarketListing& InListing, bool bOwnListing)
{
	Listing = InListing;
	bIsOwnListing = bOwnListing;
	RefreshEntry();
}

void UTDMarketListingEntryWidget::RefreshEntry()
{
	const APlayerController* Controller = GetOwningPlayer();
	const ATDPlayerState* State = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;
	const UTDInventoryComponent* Inventory = State ? State->GetInventoryComponent() : nullptr;

	// 매물에는 이름·아이콘이 없다. ItemId 로 정의를 찾아야 그릴 수 있다 —
	// 이름까지 복제하면 같은 문자열이 매물 수만큼 네트워크를 탄다.
	const FTDItemRow* Definition = Inventory != nullptr
		? Inventory->FindItemDefinition(Listing.Item.ItemId)
		: nullptr;

	if (TXT_ItemName)
	{
		FText Name = Definition != nullptr && !Definition->DisplayName.IsEmpty()
			? Definition->DisplayName
			: FText::FromName(Listing.Item.ItemId);

		// 강화 단수와 수량은 이름 옆에 붙인다. 칸을 따로 두면 대부분 비어 있다.
		if (Listing.Item.EnhanceLevel > 0)
		{
			Name = FText::Format(LOCTEXT("NameWithEnhance", "{0} +{1}"),
				Name, FText::AsNumber(Listing.Item.EnhanceLevel));
		}

		if (Listing.Item.Count > 1)
		{
			Name = FText::Format(LOCTEXT("NameWithCount", "{0} ×{1}"),
				Name, FText::AsNumber(Listing.Item.Count));
		}

		TXT_ItemName->SetText(Name);
	}

	if (TXT_SellerName)
	{
		TXT_SellerName->SetText(FText::FromString(Listing.SellerName));
	}

	if (TXT_Price)
	{
		TXT_Price->SetText(
			FText::Format(LOCTEXT("Price", "{0} G"), FText::AsNumber(Listing.Price)));
	}

	if (IMG_ItemIcon)
	{
		UTexture2D* Texture = Definition != nullptr ? Definition->Icon.LoadSynchronous() : nullptr;

		IMG_ItemIcon->SetBrushFromTexture(Texture, false);
		IMG_ItemIcon->SetRenderOpacity(Texture ? 1.0f : 0.0f);
	}

	if (TXT_ActionLabel)
	{
		TXT_ActionLabel->SetText(bIsOwnListing
			? LOCTEXT("Cancel", "내리기")
			: LOCTEXT("Buy", "구매"));
	}

	// 개체를 통째로 넘긴다 — 강화 단수와 추가 옵션까지 툴팁에 나온다.
	// 거래소에서 무엇을 사는지 확인할 수 있어야 하므로 이것이 핵심이다.
	UTDItemTooltipWidget::AttachItemInstance(this, Listing.Item);
}

void UTDMarketListingEntryWidget::HandleClicked()
{
	OnSelected.Broadcast(Listing.ListingId);
}

#undef LOCTEXT_NAMESPACE
