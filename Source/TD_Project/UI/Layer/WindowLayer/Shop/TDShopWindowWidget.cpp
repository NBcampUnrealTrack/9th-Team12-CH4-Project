#include "UI/Layer/WindowLayer/Shop/TDShopWindowWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Components/WidgetSwitcher.h"
#include "Settings/TDShopSettings.h"
#include "Shop/TDShopServiceComponent.h"
#include "UI/Layer/WindowLayer/Inventory/TDInventorySlotListItem.h"

namespace
{
	const FLinearColor ActiveTabColor(0.22f, 0.55f, 0.31f, 1.0f);
	const FLinearColor InactiveTabColor(0.40f, 0.40f, 0.40f, 1.0f);
}

void UTDShopWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BTN_BuyTab->OnClicked.AddUniqueDynamic(
		this,
		&ThisClass::HandleBuyTabClicked);
	BTN_SellTab->OnClicked.AddUniqueDynamic(
		this,
		&ThisClass::HandleSellTabClicked);
	BTN_BuyClear->OnClicked.AddUniqueDynamic(
		this,
		&ThisClass::HandleBuyClearClicked);
	BTN_Buy->OnClicked.AddUniqueDynamic(
		this,
		&ThisClass::HandleBuyClicked);
	BTN_SellClear->OnClicked.AddUniqueDynamic(
		this,
		&ThisClass::HandleSellClearClicked);
	BTN_Sell->OnClicked.AddUniqueDynamic(
		this,
		&ThisClass::HandleSellClicked);

	TV_BuyItems->OnItemClicked().RemoveAll(this);
	TV_BuyItems->OnItemClicked().AddUObject(
		this,
		&ThisClass::HandleBuyItemClicked);
	TV_BuyCart->OnItemClicked().RemoveAll(this);
	TV_BuyCart->OnItemClicked().AddUObject(
		this,
		&ThisClass::HandleBuyCartItemClicked);
	TV_SellItems->OnItemClicked().RemoveAll(this);
	TV_SellItems->OnItemClicked().AddUObject(
		this,
		&ThisClass::HandleSellItemClicked);
	TV_SellList->OnItemClicked().RemoveAll(this);
	TV_SellList->OnItemClicked().AddUObject(
		this,
		&ThisClass::HandleSellListItemClicked);

	TV_BuyItems->SetSelectionMode(ESelectionMode::None);
	TV_BuyCart->SetSelectionMode(ESelectionMode::None);
	TV_SellItems->SetSelectionMode(ESelectionMode::None);
	TV_SellList->SetSelectionMode(ESelectionMode::None);

	RefreshWindow();
}

void UTDShopWindowWidget::NativeDestruct()
{
	BTN_BuyTab->OnClicked.RemoveDynamic(
		this,
		&ThisClass::HandleBuyTabClicked);
	BTN_SellTab->OnClicked.RemoveDynamic(
		this,
		&ThisClass::HandleSellTabClicked);
	BTN_BuyClear->OnClicked.RemoveDynamic(
		this,
		&ThisClass::HandleBuyClearClicked);
	BTN_Buy->OnClicked.RemoveDynamic(
		this,
		&ThisClass::HandleBuyClicked);
	BTN_SellClear->OnClicked.RemoveDynamic(
		this,
		&ThisClass::HandleSellClearClicked);
	BTN_Sell->OnClicked.RemoveDynamic(
		this,
		&ThisClass::HandleSellClicked);

	TV_BuyItems->OnItemClicked().RemoveAll(this);
	TV_BuyCart->OnItemClicked().RemoveAll(this);
	TV_SellItems->OnItemClicked().RemoveAll(this);
	TV_SellList->OnItemClicked().RemoveAll(this);

	BuyItemObjects.Reset();
	BuyCartObjects.Reset();
	SellItemObjects.Reset();
	SellListObjects.Reset();

	Super::NativeDestruct();
}

void UTDShopWindowWidget::InitializeService(
	UTDShopServiceComponent* InService)
{
	Service = InService;
}

void UTDShopWindowWidget::ApplyView(const FTDShopWindowView& InView)
{
	if (InView.bClearCart)
	{
		BuyCartCounts.Reset();
	}

	if (InView.bClearSellList)
	{
		SellCountsBySlot.Reset();
	}

	if (PendingRequestId > 0
		&& InView.ReplyRequestId == PendingRequestId)
	{
		PendingRequestId = 0;
	}

	CurrentView = InView;
	SanitizeSellSelection();
	RefreshWindow();

	if (!InView.Message.IsEmpty())
	{
		SetStatusMessage(InView.Message);
	}
}

void UTDShopWindowWidget::RefreshWindow()
{
	SetWindowTitle(
		CurrentView.ShopName.IsEmpty()
			? FText::FromString(TEXT("상점"))
			: CurrentView.ShopName);

	TXT_Gold->SetText(
		FText::Format(
			FText::FromString(TEXT("현재 골드: {0} G")),
			FText::AsNumber(CurrentView.Gold)));

	WS_ShopPages->SetActiveWidgetIndex(ActivePageIndex);
	RefreshBuyItems();
	RefreshBuyCart();
	RefreshSellItems();
	RefreshSellList();
	RefreshButtons();
	RefreshTabVisuals();
}

const FTDShopBuyItemView* UTDShopWindowWidget::FindBuyItem(
	FName ItemId) const
{
	return CurrentView.BuyItems.FindByPredicate(
		[ItemId](const FTDShopBuyItemView& Item)
		{
			return Item.ItemId == ItemId;
		});
}

const FTDShopSellItemView* UTDShopWindowWidget::FindSellItem(
	int32 InventorySlot) const
{
	return CurrentView.SellItems.FindByPredicate(
		[InventorySlot](const FTDShopSellItemView& Item)
		{
			return Item.SlotIndex == InventorySlot;
		});
}

int64 UTDShopWindowWidget::GetBuyTotal() const
{
	int64 Total = 0;
	for (const TPair<FName, int32>& Pair : BuyCartCounts)
	{
		if (const FTDShopBuyItemView* Item = FindBuyItem(Pair.Key))
		{
			Total += static_cast<int64>(Item->Price) * Pair.Value;
		}
	}
	return Total;
}

int64 UTDShopWindowWidget::GetSellTotal() const
{
	int64 Total = 0;
	for (const TPair<int32, int32>& Pair : SellCountsBySlot)
	{
		if (const FTDShopSellItemView* Item = FindSellItem(Pair.Key))
		{
			Total += static_cast<int64>(Item->UnitSellPrice) * Pair.Value;
		}
	}
	return Total;
}

int32 UTDShopWindowWidget::GetBuySelectionKindCount() const
{
	int32 KindCount = 0;
	for (const TPair<FName, int32>& Pair : BuyCartCounts)
	{
		if (Pair.Value <= 0)
		{
			continue;
		}

		if (const FTDShopBuyItemView* Source = FindBuyItem(Pair.Key))
		{
			KindCount += Source->bStackable ? 1 : Pair.Value;
		}
	}
	return KindCount;
}

int32 UTDShopWindowWidget::GetSellSelectionKindCount() const
{
	TSet<FName> StackableTypes;
	int32 UniqueInstanceCount = 0;

	for (const TPair<int32, int32>& Pair : SellCountsBySlot)
	{
		if (Pair.Value <= 0)
		{
			continue;
		}

		if (const FTDShopSellItemView* Source = FindSellItem(Pair.Key))
		{
			if (Source->bStackable)
			{
				StackableTypes.Add(Source->ItemId);
			}
			else
			{
				UniqueInstanceCount += Pair.Value;
			}
		}
	}

	return StackableTypes.Num() + UniqueInstanceCount;
}

int32 UTDShopWindowWidget::GetMaximumTradeCount() const
{
	const UTDShopSettings* Settings = UTDShopSettings::Get();
	return FMath::Max(1, Settings ? Settings->MaxCountPerTrade : 1);
}

void UTDShopWindowWidget::SetStatusMessage(const FString& Message)
{
	TXT_Result->SetText(FText::FromString(Message));
}

UTDInventorySlotListItem* UTDShopWindowWidget::MakeBlankSlot(
	int32 SlotIndex)
{
	UTDInventorySlotListItem* Item =
		NewObject<UTDInventorySlotListItem>(this);
	Item->SlotIndex = SlotIndex;
	Item->bIsPreviewItem = true;
	return Item;
}

UTDInventorySlotListItem* UTDShopWindowWidget::MakeBuySlot(
	int32 SlotIndex,
	const FTDShopBuyItemView& Source,
	int32 Count,
	bool bCartSlot)
{
	UTDInventorySlotListItem* Item = MakeBlankSlot(SlotIndex);
	Item->bHasItem = true;
	Item->ItemInstance.SlotIndex = SlotIndex;
	Item->ItemInstance.ItemId = Source.ItemId;
	Item->ItemInstance.Count = FMath::Max(1, Count);
	Item->DisplayName = Source.DisplayName;
	Item->Icon = Source.Icon;
	Item->Rarity = Source.Rarity;
	Item->ItemType = Source.ItemType;
	Item->InteractionHint = bCartSlot
		? FText::Format(
			FText::FromString(
				TEXT("{0} G × {1}개 = {2} G\n클릭: 1개 빼기")),
			FText::AsNumber(Source.Price),
			FText::AsNumber(Count),
			FText::AsNumber(
				static_cast<int64>(Source.Price) * Count))
		: FText::Format(
			FText::FromString(TEXT("구매 가격: {0} G\n클릭: 1개 담기")),
			FText::AsNumber(Source.Price));
	Item->OnDirectClick.AddUObject(
		this,
		bCartSlot
			? &ThisClass::HandleBuyCartItemClicked
			: &ThisClass::HandleBuyItemClicked);
	return Item;
}

UTDInventorySlotListItem* UTDShopWindowWidget::MakeSellSlot(
	const FTDShopSellItemView& Source,
	int32 Count,
	bool bSelectedSlot)
{
	UTDInventorySlotListItem* Item = MakeBlankSlot(Source.SlotIndex);
	Item->bHasItem = true;
	Item->ItemInstance.SlotIndex = Source.SlotIndex;
	Item->ItemInstance.ItemId = Source.ItemId;
	Item->ItemInstance.EnhanceLevel = Source.EnhanceLevel;
	Item->ItemInstance.Count = FMath::Max(1, Count);
	Item->DisplayName = Source.DisplayName;
	Item->Icon = Source.Icon;
	Item->Rarity = Source.Rarity;
	Item->ItemType = Source.ItemType;
	Item->bInteractionEnabled = Source.bCanSell;

	if (!Source.bCanSell)
	{
		Item->InteractionHint =
			FText::FromString(TEXT("퀘스트 아이템은 판매할 수 없습니다."));
	}
	else if (bSelectedSlot)
	{
		Item->InteractionHint = FText::Format(
			FText::FromString(
				TEXT("{0} G × {1}개 = {2} G\n클릭: 1개 빼기")),
			FText::AsNumber(Source.UnitSellPrice),
			FText::AsNumber(Count),
			FText::AsNumber(
				static_cast<int64>(Source.UnitSellPrice) * Count));
	}
	else
	{
		Item->InteractionHint = FText::Format(
			FText::FromString(TEXT("판매 가격: {0} G\n클릭: 1개 담기")),
			FText::AsNumber(Source.UnitSellPrice));
	}

	if (Source.bCanSell)
	{
		Item->OnDirectClick.AddUObject(
			this,
			bSelectedSlot
				? &ThisClass::HandleSellListItemClicked
				: &ThisClass::HandleSellItemClicked);
	}

	return Item;
}

void UTDShopWindowWidget::SetTileItems(
	UTileView* TileView,
	TArray<TObjectPtr<UTDInventorySlotListItem>>& Storage,
	TArray<UTDInventorySlotListItem*>& Items)
{
	Storage.Reset();
	Storage.Reserve(Items.Num());

	TArray<UObject*> ListItems;
	ListItems.Reserve(Items.Num());

	for (UTDInventorySlotListItem* Item : Items)
	{
		Storage.Add(Item);
		ListItems.Add(Item);
	}

	TileView->SetListItems(ListItems);
}

void UTDShopWindowWidget::RefreshBuyItems()
{
	TArray<UTDInventorySlotListItem*> Items;
	Items.Reserve(CurrentView.BuyItems.Num());

	for (int32 Index = 0; Index < CurrentView.BuyItems.Num(); ++Index)
	{
		Items.Add(MakeBuySlot(
			Index,
			CurrentView.BuyItems[Index],
			1,
			false));
	}

	SetTileItems(TV_BuyItems, BuyItemObjects, Items);
}

void UTDShopWindowWidget::RefreshBuyCart()
{
	TArray<FName> InvalidItems;
	for (const TPair<FName, int32>& Pair : BuyCartCounts)
	{
		if (Pair.Value <= 0 || !FindBuyItem(Pair.Key))
		{
			InvalidItems.Add(Pair.Key);
		}
	}
	for (FName ItemId : InvalidItems)
	{
		BuyCartCounts.Remove(ItemId);
	}

	TArray<UTDInventorySlotListItem*> Items;
	Items.Reserve(SelectionSlotCount);

	for (const FTDShopBuyItemView& Source : CurrentView.BuyItems)
	{
		const int32* Count = BuyCartCounts.Find(Source.ItemId);
		if (Count && *Count > 0)
		{
			if (Source.bStackable)
			{
				Items.Add(MakeBuySlot(
					Items.Num(),
					Source,
					*Count,
					true));
			}
			else
			{
				// 장신구는 같은 ItemId여도 구매할 개수만큼 별도 칸입니다.
				for (int32 Index = 0; Index < *Count; ++Index)
				{
					Items.Add(MakeBuySlot(
						Items.Num(),
						Source,
						1,
						true));
				}
			}
		}
	}

	while (Items.Num() < SelectionSlotCount)
	{
		Items.Add(MakeBlankSlot(Items.Num()));
	}

	SetTileItems(TV_BuyCart, BuyCartObjects, Items);
	TXT_BuyTotal->SetText(
		FText::Format(
			FText::FromString(TEXT("필요 골드: {0} G")),
			FText::AsNumber(GetBuyTotal())));
}

void UTDShopWindowWidget::RefreshSellItems()
{
	TArray<UTDInventorySlotListItem*> Items;
	Items.Reserve(CurrentView.SellItems.Num());

	for (const FTDShopSellItemView& Source : CurrentView.SellItems)
	{
		const int32 SelectedCount = SellCountsBySlot.FindRef(Source.SlotIndex);
		const int32 RemainingCount = Source.Count - SelectedCount;
		if (RemainingCount > 0)
		{
			Items.Add(MakeSellSlot(
				Source,
				RemainingCount,
				false));
		}
	}

	SetTileItems(TV_SellItems, SellItemObjects, Items);
}

void UTDShopWindowWidget::RefreshSellList()
{
	TArray<UTDInventorySlotListItem*> Items;
	Items.Reserve(SelectionSlotCount);
	TMap<FName, int32> SelectedStackableCountsByItem;

	// 서버 요청은 실제 인벤토리 슬롯별 수량으로 보관합니다. 화면에서는
	// 스택 가능한 아이템만 같은 ItemId끼리 한 칸으로 합칩니다.
	for (const TPair<int32, int32>& Pair : SellCountsBySlot)
	{
		if (Pair.Value <= 0)
		{
			continue;
		}

		if (const FTDShopSellItemView* Source = FindSellItem(Pair.Key);
			Source && Source->bStackable)
		{
			SelectedStackableCountsByItem.FindOrAdd(Source->ItemId) += Pair.Value;
		}
	}

	TSet<FName> AddedStackableTypes;
	for (const FTDShopSellItemView& Source : CurrentView.SellItems)
	{
		if (Source.bStackable)
		{
			if (AddedStackableTypes.Contains(Source.ItemId))
			{
				continue;
			}

			const int32 Count =
				SelectedStackableCountsByItem.FindRef(Source.ItemId);
			if (Count > 0)
			{
				Items.Add(MakeSellSlot(Source, Count, true));
				AddedStackableTypes.Add(Source.ItemId);
			}
		}
		else
		{
			const int32 Count = SellCountsBySlot.FindRef(Source.SlotIndex);
			if (Count > 0)
			{
				// 장신구처럼 겹칠 수 없는 아이템은 인스턴스마다 한 칸입니다.
				Items.Add(MakeSellSlot(Source, Count, true));
			}
		}
	}

	while (Items.Num() < SelectionSlotCount)
	{
		Items.Add(MakeBlankSlot(INDEX_NONE));
	}

	SetTileItems(TV_SellList, SellListObjects, Items);
	TXT_SellTotal->SetText(
		FText::Format(
			FText::FromString(TEXT("획득 골드: {0} G")),
			FText::AsNumber(GetSellTotal())));
}

void UTDShopWindowWidget::SanitizeSellSelection()
{
	TArray<int32> InvalidSlots;
	for (TPair<int32, int32>& Pair : SellCountsBySlot)
	{
		const FTDShopSellItemView* Source = FindSellItem(Pair.Key);
		if (!Source || !Source->bCanSell || Source->Count <= 0)
		{
			InvalidSlots.Add(Pair.Key);
			continue;
		}

		Pair.Value = FMath::Clamp(Pair.Value, 1, Source->Count);
	}

	for (int32 SlotIndex : InvalidSlots)
	{
		SellCountsBySlot.Remove(SlotIndex);
	}
}

void UTDShopWindowWidget::RefreshButtons()
{
	const bool bIdle = PendingRequestId == 0;
	const int64 BuyTotal = GetBuyTotal();

	BTN_Buy->SetIsEnabled(
		bIdle
		&& !BuyCartCounts.IsEmpty()
		&& BuyTotal >= 0
		&& BuyTotal <= CurrentView.Gold);
	BTN_BuyClear->SetIsEnabled(
		bIdle && !BuyCartCounts.IsEmpty());
	BTN_Sell->SetIsEnabled(
		bIdle && !SellCountsBySlot.IsEmpty());
	BTN_SellClear->SetIsEnabled(
		bIdle && !SellCountsBySlot.IsEmpty());

	TV_BuyItems->SetIsEnabled(bIdle);
	TV_BuyCart->SetIsEnabled(bIdle);
	TV_SellItems->SetIsEnabled(bIdle);
	TV_SellList->SetIsEnabled(bIdle);
}

void UTDShopWindowWidget::RefreshTabVisuals()
{
	BTN_BuyTab->SetBackgroundColor(
		ActivePageIndex == 0 ? ActiveTabColor : InactiveTabColor);
	BTN_SellTab->SetBackgroundColor(
		ActivePageIndex == 1 ? ActiveTabColor : InactiveTabColor);
}

void UTDShopWindowWidget::HandleBuyItemClicked(UObject* ItemObject)
{
	UTDInventorySlotListItem* Item =
		Cast<UTDInventorySlotListItem>(ItemObject);
	if (PendingRequestId != 0 || !Item || !Item->bHasItem)
	{
		return;
	}

	const FName ItemId = Item->ItemInstance.ItemId;
	const FTDShopBuyItemView* Source = FindBuyItem(ItemId);
	if (!Source)
	{
		return;
	}

	int32* ExistingCount = BuyCartCounts.Find(ItemId);
	const bool bConsumesNewKind = !Source->bStackable || !ExistingCount;
	if (bConsumesNewKind
		&& GetBuySelectionKindCount() >= SelectionSlotCount)
	{
		SetStatusMessage(TEXT("장바구니에는 최대 4종류까지 담을 수 있습니다."));
		return;
	}

	int32& Count = BuyCartCounts.FindOrAdd(ItemId);
	const int32 MaximumCount = Source->bStackable
		? GetMaximumTradeCount()
		: SelectionSlotCount;
	Count = FMath::Min(Count + 1, MaximumCount);

	RefreshBuyCart();
	RefreshButtons();
}

void UTDShopWindowWidget::HandleBuyCartItemClicked(UObject* ItemObject)
{
	UTDInventorySlotListItem* Item =
		Cast<UTDInventorySlotListItem>(ItemObject);
	if (PendingRequestId != 0 || !Item || !Item->bHasItem)
	{
		return;
	}

	if (int32* Count = BuyCartCounts.Find(Item->ItemInstance.ItemId))
	{
		--(*Count);
		if (*Count <= 0)
		{
			BuyCartCounts.Remove(Item->ItemInstance.ItemId);
		}
	}

	RefreshBuyCart();
	RefreshButtons();
}

void UTDShopWindowWidget::HandleSellItemClicked(UObject* ItemObject)
{
	UTDInventorySlotListItem* Item =
		Cast<UTDInventorySlotListItem>(ItemObject);
	if (PendingRequestId != 0
		|| !Item
		|| !Item->bHasItem
		|| !Item->bInteractionEnabled)
	{
		return;
	}

	const int32 InventorySlot = Item->SlotIndex;
	const FTDShopSellItemView* Source = FindSellItem(InventorySlot);
	if (!Source || !Source->bCanSell)
	{
		return;
	}

	int32 SelectedCountForItem = 0;
	bool bStackableTypeAlreadySelected = false;
	for (const TPair<int32, int32>& Pair : SellCountsBySlot)
	{
		if (Pair.Value <= 0)
		{
			continue;
		}

		if (const FTDShopSellItemView* SelectedSource = FindSellItem(Pair.Key))
		{
			if (SelectedSource->ItemId == Source->ItemId)
			{
				SelectedCountForItem += Pair.Value;
				bStackableTypeAlreadySelected |= SelectedSource->bStackable;
			}
		}
	}

	const bool bConsumesNewKind =
		!Source->bStackable || !bStackableTypeAlreadySelected;
	if (bConsumesNewKind
		&& GetSellSelectionKindCount() >= SelectionSlotCount)
	{
		SetStatusMessage(TEXT("판매 목록에는 최대 4종류까지 담을 수 있습니다."));
		return;
	}

	const int32 ExistingSlotCount = SellCountsBySlot.FindRef(InventorySlot);
	if (ExistingSlotCount >= Source->Count
		|| (Source->bStackable
			&& SelectedCountForItem >= GetMaximumTradeCount()))
	{
		return;
	}

	int32& Count = SellCountsBySlot.FindOrAdd(InventorySlot);
	++Count;

	RefreshSellItems();
	RefreshSellList();
	RefreshButtons();
}

void UTDShopWindowWidget::HandleSellListItemClicked(UObject* ItemObject)
{
	UTDInventorySlotListItem* Item =
		Cast<UTDInventorySlotListItem>(ItemObject);
	if (PendingRequestId != 0 || !Item || !Item->bHasItem)
	{
		return;
	}

	const FName ItemId = Item->ItemInstance.ItemId;
	const FTDShopSellItemView* ClickedSource = FindSellItem(Item->SlotIndex);
	if (ClickedSource && !ClickedSource->bStackable)
	{
		if (int32* Count = SellCountsBySlot.Find(ClickedSource->SlotIndex))
		{
			--(*Count);
			if (*Count <= 0)
			{
				SellCountsBySlot.Remove(ClickedSource->SlotIndex);
			}
		}
	}
	else
	{
		for (int32 Index = CurrentView.SellItems.Num() - 1; Index >= 0; --Index)
		{
			const FTDShopSellItemView& Source = CurrentView.SellItems[Index];
			if (!Source.bStackable || Source.ItemId != ItemId)
			{
				continue;
			}

			if (int32* Count = SellCountsBySlot.Find(Source.SlotIndex))
			{
				--(*Count);
				if (*Count <= 0)
				{
					SellCountsBySlot.Remove(Source.SlotIndex);
				}
				break;
			}
		}
	}

	RefreshSellItems();
	RefreshSellList();
	RefreshButtons();
}

void UTDShopWindowWidget::HandleBuyTabClicked()
{
	ActivePageIndex = 0;
	WS_ShopPages->SetActiveWidgetIndex(ActivePageIndex);
	RefreshTabVisuals();
}

void UTDShopWindowWidget::HandleSellTabClicked()
{
	ActivePageIndex = 1;
	WS_ShopPages->SetActiveWidgetIndex(ActivePageIndex);
	RefreshTabVisuals();
}

void UTDShopWindowWidget::HandleBuyClearClicked()
{
	if (PendingRequestId == 0)
	{
		BuyCartCounts.Reset();
		RefreshBuyCart();
		RefreshButtons();
	}
}

void UTDShopWindowWidget::HandleSellClearClicked()
{
	if (PendingRequestId == 0)
	{
		SellCountsBySlot.Reset();
		RefreshSellItems();
		RefreshSellList();
		RefreshButtons();
	}
}

void UTDShopWindowWidget::HandleBuyClicked()
{
	if (!Service.IsValid()
		|| PendingRequestId != 0
		|| BuyCartCounts.IsEmpty()
		|| RequestCounter >= MAX_int32)
	{
		return;
	}

	TArray<FTDShopCartLine> Lines;
	for (const FTDShopBuyItemView& Source : CurrentView.BuyItems)
	{
		if (const int32* Count = BuyCartCounts.Find(Source.ItemId))
		{
			FTDShopCartLine& Line = Lines.AddDefaulted_GetRef();
			Line.ItemId = Source.ItemId;
			Line.Count = *Count;
		}
	}

	++RequestCounter;
	PendingRequestId = RequestCounter;
	RefreshButtons();
	SetStatusMessage(TEXT("구매 내용을 확인하고 있습니다..."));

	Service->ServerBuyCart(
		CurrentView.SessionId,
		CurrentView.InventoryRevision,
		Lines,
		PendingRequestId);
}

void UTDShopWindowWidget::HandleSellClicked()
{
	if (!Service.IsValid()
		|| PendingRequestId != 0
		|| SellCountsBySlot.IsEmpty()
		|| RequestCounter >= MAX_int32)
	{
		return;
	}

	TArray<FTDShopSellLine> Lines;
	for (const FTDShopSellItemView& Source : CurrentView.SellItems)
	{
		if (const int32* Count = SellCountsBySlot.Find(Source.SlotIndex))
		{
			FTDShopSellLine& Line = Lines.AddDefaulted_GetRef();
			Line.SlotIndex = Source.SlotIndex;
			Line.Count = *Count;
		}
	}

	++RequestCounter;
	PendingRequestId = RequestCounter;
	RefreshButtons();
	SetStatusMessage(TEXT("판매 내용을 확인하고 있습니다..."));

	Service->ServerSellCart(
		CurrentView.SessionId,
		CurrentView.InventoryRevision,
		Lines,
		PendingRequestId);
}
