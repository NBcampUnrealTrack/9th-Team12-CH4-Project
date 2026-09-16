#include "UI/Layer/WindowLayer/Market/TDMarketWindowWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Items/TDInventoryComponent.h"
#include "Player/TDPlayerController.h"
#include "Player/TDPlayerState.h"
#include "UI/Layer/WindowLayer/Market/TDMarketListingEntryWidget.h"

#define LOCTEXT_NAMESPACE "TDMarketWindowWidget"

namespace
{
	/** 거래소 결과를 사람이 읽는 문구로. 실패 이유를 그대로 보여줘야 다음에 뭘 할지 알 수 있다. */
	FText MarketResultText(ETDMarketResult Result)
	{
		switch (Result)
		{
		case ETDMarketResult::Success:
			return LOCTEXT("Success", "처리했습니다.");
		case ETDMarketResult::ListingNotFound:
			return LOCTEXT("NotFound", "이미 팔렸거나 내려간 매물입니다. 목록을 새로 불러왔습니다.");
		case ETDMarketResult::NotEnoughGold:
			return LOCTEXT("NoGold", "골드가 부족합니다.");
		case ETDMarketResult::InventoryFull:
			return LOCTEXT("Full", "가방에 빈 자리가 없습니다.");
		case ETDMarketResult::CannotBuyOwnListing:
			return LOCTEXT("OwnListing", "자기 매물은 살 수 없습니다. 내 판매 탭에서 내릴 수 있습니다.");
		case ETDMarketResult::NotSeller:
			return LOCTEXT("NotSeller", "남의 매물은 내릴 수 없습니다.");
		case ETDMarketResult::ItemNotFound:
			return LOCTEXT("ItemNotFound", "그 칸에 아이템이 없습니다.");
		case ETDMarketResult::InvalidPrice:
			return LOCTEXT("BadPrice", "가격이 올바르지 않습니다.");
		case ETDMarketResult::TooManyListings:
			return LOCTEXT("TooMany", "등록 한도를 넘었습니다. 올린 매물을 먼저 정리하세요.");
		case ETDMarketResult::ItemNotTradable:
			return LOCTEXT("NotTradable", "거래할 수 없는 아이템입니다.");
		case ETDMarketResult::NoCharacterSelected:
			return LOCTEXT("NoCharacter", "캐릭터를 먼저 선택해야 합니다.");
		default:
			return LOCTEXT("Internal", "처리하지 못했습니다. 데이터를 확인하세요.");
		}
	}
}

ATDPlayerController* UTDMarketWindowWidget::GetMarketController() const
{
	return Cast<ATDPlayerController>(GetOwningPlayer());
}

void UTDMarketWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetWindowTitle(LOCTEXT("Title", "거래소"));

	if (BTN_TabBuy)
	{
		BTN_TabBuy->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTabBuyClicked);
	}
	if (BTN_TabMine)
	{
		BTN_TabMine->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTabMineClicked);
	}
	if (BTN_Search)
	{
		BTN_Search->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSearchClicked);
	}
	if (BTN_ListItem)
	{
		BTN_ListItem->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleListItemClicked);
	}

	if (ATDPlayerController* Controller = GetMarketController())
	{
		Controller->OnMarketResult.AddUniqueDynamic(this, &ThisClass::HandleMarketResult);
		Controller->OnMarketSearchResult.AddUniqueDynamic(this, &ThisClass::HandleSearchResult);

		if (const ATDPlayerState* State = Controller->GetPlayerState<ATDPlayerState>())
		{
			MyName = State->GetPlayerName();
		}
	}

	RefreshGold();
	ShowTab(ETDMarketTab::Buy);
}

void UTDMarketWindowWidget::NativeDestruct()
{
	if (BTN_TabBuy)
	{
		BTN_TabBuy->OnClicked.RemoveDynamic(this, &ThisClass::HandleTabBuyClicked);
	}
	if (BTN_TabMine)
	{
		BTN_TabMine->OnClicked.RemoveDynamic(this, &ThisClass::HandleTabMineClicked);
	}
	if (BTN_Search)
	{
		BTN_Search->OnClicked.RemoveDynamic(this, &ThisClass::HandleSearchClicked);
	}
	if (BTN_ListItem)
	{
		BTN_ListItem->OnClicked.RemoveDynamic(this, &ThisClass::HandleListItemClicked);
	}

	if (ATDPlayerController* Controller = GetMarketController())
	{
		Controller->OnMarketResult.RemoveDynamic(this, &ThisClass::HandleMarketResult);
		Controller->OnMarketSearchResult.RemoveDynamic(this, &ThisClass::HandleSearchResult);
	}

	Super::NativeDestruct();
}

void UTDMarketWindowWidget::ShowTab(ETDMarketTab Tab)
{
	CurrentTab = Tab;

	if (WS_Tabs)
	{
		WS_Tabs->SetActiveWidgetIndex(Tab == ETDMarketTab::Buy ? 0 : 1);
	}

	RefreshCurrentTab();
}

void UTDMarketWindowWidget::RefreshCurrentTab()
{
	ATDPlayerController* Controller = GetMarketController();
	if (Controller == nullptr)
	{
		return;
	}

	if (CurrentTab == ETDMarketTab::Buy)
	{
		const FString Filter = ETB_SearchFilter
			? ETB_SearchFilter->GetText().ToString().TrimStartAndEnd()
			: FString();

		// 비우면 전부. 넣으면 그 아이템만.
		Controller->ServerSearchListings(Filter.IsEmpty() ? NAME_None : FName(*Filter), 0);
		return;
	}

	Controller->ServerRequestMyListings();
}

void UTDMarketWindowWidget::RefreshGold()
{
	if (TXT_Gold == nullptr)
	{
		return;
	}

	const ATDPlayerController* Controller = GetMarketController();
	const ATDPlayerState* State = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;
	const UTDInventoryComponent* Inventory = State ? State->GetInventoryComponent() : nullptr;

	TXT_Gold->SetText(FText::Format(LOCTEXT("Gold", "보유 골드: {0} G"),
		FText::AsNumber(Inventory ? Inventory->GetGold() : 0)));
}

void UTDMarketWindowWidget::RebuildList(UScrollBox* Target,
	const TArray<FTDMarketListing>& Listings, bool bOwnListings, UTextBlock* EmptyLabel)
{
	if (Target == nullptr)
	{
		return;
	}

	Target->ClearChildren();

	if (EmptyLabel)
	{
		EmptyLabel->SetVisibility(Listings.IsEmpty()
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	if (ListingEntryClass == nullptr)
	{
		if (TXT_Result)
		{
			TXT_Result->SetText(
				LOCTEXT("NoEntryClass", "Listing Entry Class 에 WBP_MarketListingEntry 를 지정하세요."));
		}
		return;
	}

	for (const FTDMarketListing& Listing : Listings)
	{
		UTDMarketListingEntryWidget* Entry =
			CreateWidget<UTDMarketListingEntryWidget>(GetOwningPlayer(), ListingEntryClass);

		if (Entry == nullptr)
		{
			continue;
		}

		// 구매 탭에도 내 매물이 섞여 들어온다. 그 줄은 "내리기" 로 보여 준다 —
		// 사려고 눌러도 서버가 CannotBuyOwnListing 으로 막기 때문에 그편이 덜 헷갈린다.
		const bool bOwn = bOwnListings || (!MyName.IsEmpty() && Listing.SellerName == MyName);

		Entry->SetupEntry(Listing, bOwn);
		Entry->OnSelected.AddUniqueDynamic(this, &ThisClass::HandleEntrySelected);

		Target->AddChild(Entry);
	}
}

void UTDMarketWindowWidget::HandleTabBuyClicked()
{
	ShowTab(ETDMarketTab::Buy);
}

void UTDMarketWindowWidget::HandleTabMineClicked()
{
	ShowTab(ETDMarketTab::Mine);
}

void UTDMarketWindowWidget::HandleSearchClicked()
{
	RefreshCurrentTab();
}

void UTDMarketWindowWidget::HandleListItemClicked()
{
	ATDPlayerController* Controller = GetMarketController();
	if (Controller == nullptr || ETB_ListSlot == nullptr || ETB_ListPrice == nullptr)
	{
		return;
	}

	const FString SlotText = ETB_ListSlot->GetText().ToString().TrimStartAndEnd();
	const FString PriceText = ETB_ListPrice->GetText().ToString().TrimStartAndEnd();

	if (SlotText.IsEmpty() || PriceText.IsEmpty() || !SlotText.IsNumeric() || !PriceText.IsNumeric())
	{
		if (TXT_Result)
		{
			TXT_Result->SetText(LOCTEXT("BadInput", "칸 번호와 가격을 숫자로 입력하세요."));
		}
		return;
	}

	// 검사는 서버가 다시 한다. 여기서 막는 것은 숫자가 아닌 입력뿐이다.
	Controller->ServerListItem(FCString::Atoi(*SlotText), FCString::Atoi(*PriceText));
}

void UTDMarketWindowWidget::HandleEntrySelected(int32 ListingId)
{
	ATDPlayerController* Controller = GetMarketController();
	if (Controller == nullptr || ListingId <= 0)
	{
		return;
	}

	if (CurrentTab == ETDMarketTab::Mine)
	{
		Controller->ServerCancelListing(ListingId);
		return;
	}

	Controller->ServerBuyListing(ListingId);
}

void UTDMarketWindowWidget::HandleMarketResult(ETDMarketResult Result, int32 ListingId)
{
	if (TXT_Result)
	{
		TXT_Result->SetText(MarketResultText(Result));
	}

	// 성공이든 실패든 목록과 골드를 다시 읽는다. 특히 ListingNotFound 는
	// 화면이 낡았다는 뜻이라 새로 고치는 것이 곧 해결이다.
	RefreshGold();
	RefreshCurrentTab();

	// 등록에 성공했으면 입력칸을 비운다. 같은 칸을 또 올리려다 ItemNotFound 를 보는 일이 없다.
	if (Result == ETDMarketResult::Success && CurrentTab == ETDMarketTab::Mine)
	{
		if (ETB_ListSlot)
		{
			ETB_ListSlot->SetText(FText::GetEmpty());
		}
		if (ETB_ListPrice)
		{
			ETB_ListPrice->SetText(FText::GetEmpty());
		}
	}
}

void UTDMarketWindowWidget::HandleSearchResult(const TArray<FTDMarketListing>& Listings)
{
	// 두 탭이 같은 델리게이트를 쓴다. 지금 열린 탭에 그린다 —
	// 탭을 바꾸면 그때 다시 요청하므로 엇갈린 결과가 남지 않는다.
	if (CurrentTab == ETDMarketTab::Buy)
	{
		RebuildList(SB_Listings, Listings, /*bOwnListings=*/false, TXT_ListingsEmpty);
		return;
	}

	RebuildList(SB_MyListings, Listings, /*bOwnListings=*/true, TXT_MyListingsEmpty);
}

#undef LOCTEXT_NAMESPACE
