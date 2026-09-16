#include "TDInventoryContentWidget.h"

#include "TDInventorySlotListItem.h"
#include "TDInventoryActionPolicy.h"
#include "Core/TDGameplayTags.h"
#include "Items/TDItemUseComponent.h"
#include "Stats/TDProgressionComponent.h"
#include "Components/TileView.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Data/TDItemRow.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Items/TDInventoryComponent.h"
#include "UI/Common/ItemSlot/TDItemDragDropOperation.h"
#include "UObject/ConstructorHelpers.h"

bool UTDInventoryContentWidget::RequestItemAction(UTDInventorySlotListItem* Item)
{
	if (IsDesignTime() || InventoryOverrideTable || !IsValid(Item)
		|| Item->GetTypedOuter<UTDInventoryContentWidget>() != this
		|| Item->bIsPreviewItem || !Item->bHasItem) return false;

	APlayerController* Controller = GetOwningPlayer();
	APlayerState* State = Controller ? Controller->PlayerState.Get() : nullptr;
	if (!Controller || !Controller->IsLocalController() || !State) return false;
	BindInventoryComponent();
	UpdatePendingItemAction();
	const double Now = FPlatformTime::Seconds();
	// 수량/장착 복제가 반영되기 전에 다음 요청을 보내지 않는다. 빠른 더블클릭도 억제.
	if (bItemActionPending || Now - LastItemActionAt < 0.2) return false;
	UTDItemUseComponent* ItemUse = State->FindComponentByClass<UTDItemUseComponent>();
	if (!IsValid(InventoryComponent) || !ItemUse) return false;

	const FTDItemInstance* Source = InventoryComponent->FindBySlot(Item->SlotIndex);
	// 스크롤/복제로 재사용된 Entry가 이전 물건을 가리키는 경우 요청하지 않는다.
	if (!Source || !Source->IsValid() || !TDInventoryAction::SameItem(Source, Item->ItemInstance))
		return false;
	const FTDItemRow* Definition = InventoryComponent->FindItemDefinition(Source->ItemId);
	if (!Definition) return false;
	const bool bEquip = Definition->ItemType == TDTags::Item_Type_Accessory.GetTag();
	const bool bUse = Definition->ItemType == TDTags::Item_Type_Consumable.GetTag();
	if (!bEquip && !bUse) return false;

	const UTDProgressionComponent* Progression = State->FindComponentByClass<UTDProgressionComponent>();
	if (Definition->RequiredLevel > (Progression ? Progression->GetLevel() : 0))
	{
		UE_LOG(LogTemp, Log, TEXT("인벤토리: 요구 레벨이 부족하여 우클릭 요청을 보내지 않았습니다."));
		return false;
	}
	int32 EquipSlot = INDEX_NONE;
	if (bEquip)
	{
		EquipSlot = TDInventoryAction::FindEquipSlot(ItemUse->GetEquippedItems(), UTDItemUseComponent::EquipSlotCount);
		if (EquipSlot == INDEX_NONE) return false;
		// 기존 서버와 동일한 중복 제한. 다른 칸에 같은 종류가 있으면 자동 교체 대상으로 바꾸지 않는다.
		for (const FTDItemInstance& Equipped : ItemUse->GetEquippedItems())
		{
			if (Equipped.ItemId == Source->ItemId && Equipped.SlotIndex != EquipSlot)
			{
				UE_LOG(LogTemp, Log, TEXT("인벤토리: 같은 아이템은 중복 장착할 수 없습니다."));
				return false;
			}
		}
	}

	// Listen Server에서는 RPC가 즉시 실행될 수 있으므로 호출 전에 상태를 보관한다.
	PendingSourceItem = *Source;
	PendingEquipSlot = EquipSlot;
	PendingInventory = InventoryComponent.Get();
	PendingItemUse = ItemUse;
	ItemActionStartedAt = Now;
	LastItemActionAt = Now;
	bItemActionPending = true;
	if (bEquip) ItemUse->ServerEquipItem(Item->SlotIndex, EquipSlot);
	else ItemUse->ServerUseItem(Item->SlotIndex);
	return true;
}

void UTDInventoryContentWidget::UpdatePendingItemAction()
{
	if (!bItemActionPending) return;
	if (!PendingInventory.IsValid() || !PendingItemUse.IsValid()
		|| PendingInventory.Get() != InventoryComponent.Get())
	{
		bItemActionPending = false;
		return;
	}
	const FTDItemInstance* Current = PendingInventory->FindBySlot(PendingSourceItem.SlotIndex);
	const bool bInventoryUpdated = !TDInventoryAction::SameItem(Current, PendingSourceItem);
	// 두 FastArray는 서로 다른 시점에 도착할 수 있다. 장착 목록도 확인한 뒤 잠금을 푼다.
	const bool bEquipmentUpdated = PendingEquipSlot == INDEX_NONE
		|| TDInventoryAction::SameItem(PendingItemUse->GetEquipped(PendingEquipSlot), PendingSourceItem);
	if (bInventoryUpdated && bEquipmentUpdated)
	{
		bItemActionPending = false;
		return;
	}
	// 기존 RPC에는 실패 응답이 없다. 거절/지연을 성공으로 간주하지 않고 잠금만 해제하며 재전송하지 않는다.
	if (FPlatformTime::Seconds() - ItemActionStartedAt >= 3.0)
	{
		bItemActionPending = false;
		UE_LOG(LogTemp, Log, TEXT("인벤토리: 사용/장착 결과를 확인하지 못해 클릭 대기를 해제했습니다."));
	}
}


UTDInventoryContentWidget::UTDInventoryContentWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UDataTable> ItemTableFinder(
		TEXT("/Game/Data/DataTables/Item/DT_ItemDefinition.DT_ItemDefinition"));
	PreviewItemTable = ItemTableFinder.Object;
}

void UTDInventoryContentWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		if (IsValid(InventoryOverrideTable))
		{
			BuildInventoryFromTable(InventoryOverrideTable);
			BP_OnInventoryRefreshed();
		}
		else if (bUsePreviewInventory)
		{
			BuildPreviewInventory();
		}
	}
	RefreshFooter();
}

void UTDInventoryContentWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindInventoryComponent();
	RefreshInventory();
	if (ExpandInventoryButton)
	{
		ExpandInventoryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleExpandClicked);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SourceCheckTimer, this, &ThisClass::CheckInventorySource, 0.25f, true);
	}
}

int32 UTDInventoryContentWidget::FindSlotIndexAt(const FVector2D& ScreenPosition) const
{
	if (!IsValid(InventoryTileView))
	{
		return INDEX_NONE;
	}

	// 화면에 보이는 칸만 위젯을 갖는다. 스크롤로 벗어난 칸은 nullptr 이라 자연스럽게 걸러진다.
	for (const UTDInventorySlotListItem* Item : SlotListItems)
	{
		if (!IsValid(Item))
		{
			continue;
		}

		const UUserWidget* Entry = InventoryTileView->GetEntryWidgetFromItem(Item);
		if (!IsValid(Entry))
		{
			continue;
		}

		if (Entry->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return Item->SlotIndex;
		}
	}

	return INDEX_NONE;
}

bool UTDInventoryContentWidget::NativeOnDrop(const FGeometry& Geometry,
	const FDragDropEvent& Event, UDragDropOperation* Operation)
{
	const UTDItemDragDropOperation* ItemDrag = Cast<UTDItemDragDropOperation>(Operation);
	if (ItemDrag == nullptr)
	{
		return Super::NativeOnDrop(Geometry, Event, Operation);
	}

	// 같은 위젯을 상점의 미리보기 목록도 쓴다. 거기서는 칸을 옮기지 않는다.
	if (!IsValid(InventoryComponent) || ItemDrag->SourceInventory.Get() != InventoryComponent)
	{
		return false;
	}

	const int32 TargetSlot = FindSlotIndexAt(Event.GetScreenSpacePosition());

	// 빈 칸에도 놓을 수 있어야 하므로 아이템 유무는 보지 않는다. 비었는지 차 있는지는
	// 서버의 MoveItem 이 판단한다 — 비었으면 이동, 겹치면 합치기, 아니면 교환이다.
	if (TargetSlot == INDEX_NONE
		|| ItemDrag->SourceSlotIndex == INDEX_NONE
		|| ItemDrag->SourceSlotIndex == TargetSlot)
	{
		return false;
	}

	// 칸 번호만 넘긴다. 검증은 전부 서버 몫이다.
	InventoryComponent->ServerMoveItem(ItemDrag->SourceSlotIndex, TargetSlot);
	return true;
}

void UTDInventoryContentWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SourceCheckTimer);
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	if (ExpandInventoryButton)
	{
		ExpandInventoryButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleExpandClicked);
	}
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
		InventoryComponent->OnGoldChanged.RemoveDynamic(this, &ThisClass::HandleGoldChanged);
	}

	InventoryComponent = nullptr;
	SlotListItems.Reset();

	Super::NativeDestruct();
}

void UTDInventoryContentWidget::BindInventoryComponent()
{
	if (bUseExternalItems)
	{
		return;
	}

	UTDInventoryComponent* NewInventory = nullptr;
	if (const APlayerController* PlayerController = GetOwningPlayer())
	{
		if (const APlayerState* PlayerState = PlayerController->PlayerState)
		{
			NewInventory = PlayerState->FindComponentByClass<UTDInventoryComponent>();
		}
	}

	if (InventoryComponent == NewInventory)
	{
		return;
	}

	if (IsValid(InventoryComponent))
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
		InventoryComponent->OnGoldChanged.RemoveDynamic(this, &ThisClass::HandleGoldChanged);
	}

	InventoryComponent = NewInventory;
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);
		InventoryComponent->OnGoldChanged.AddUniqueDynamic(this, &ThisClass::HandleGoldChanged);
	}
	SetDisplayedGold(IsValid(InventoryComponent) ? InventoryComponent->GetGold() : 0);
}

void UTDInventoryContentWidget::SetInventoryOverrideTable(UDataTable* InTable)
{
	if (InventoryOverrideTable == InTable)
	{
		return;
	}

	InventoryOverrideTable = InTable;
	RefreshInventory();
}

void UTDInventoryContentWidget::RefreshInventory()
{
	if (bUseExternalItems)
	{
		BuildExternalInventory();
		return;
	}

	// WBP에서 테스트 테이블을 지정한 경우에만 실제 플레이어 인벤토리를 대체한다.
	if (IsValid(InventoryOverrideTable))
	{
		BuildInventoryFromTable(InventoryOverrideTable);
		RefreshFooter();
		BP_OnInventoryRefreshed();
		return;
	}

	if (!IsValid(InventoryComponent))
	{
		BindInventoryComponent();
	}

	SlotListItems.Reset();

	if (!IsValid(InventoryComponent) || !IsValid(InventoryTileView))
	{
		if (IsValid(InventoryTileView))
		{
			InventoryTileView->ClearListItems();
		}
		RefreshFooter();
		BP_OnInventoryRefreshed();
		return;
	}

	const int32 Capacity = InventoryComponent->GetSlotCapacity();
	SlotListItems.Reserve(Capacity);

	TMap<int32, const FTDItemInstance*> ItemsBySlot;
	ItemsBySlot.Reserve(InventoryComponent->GetUsedSlotCount());
	for (const FTDItemInstance& Item : InventoryComponent->GetItems())
	{
		if (Item.SlotIndex >= 0 && Item.SlotIndex < Capacity)
		{
			ItemsBySlot.Add(Item.SlotIndex, &Item);
		}
	}

	TArray<UObject*> ListItems;
	ListItems.Reserve(Capacity);
	for (int32 SlotIndex = 0; SlotIndex < Capacity; ++SlotIndex)
	{
		UTDInventorySlotListItem* SlotListItem = NewObject<UTDInventorySlotListItem>(this);
		SlotListItem->SlotIndex = SlotIndex;

		if (const FTDItemInstance* const* FoundItem = ItemsBySlot.Find(SlotIndex))
		{
			SlotListItem->bHasItem = true;
			SlotListItem->ItemInstance = **FoundItem;

			if (const FTDItemRow* Definition = InventoryComponent->FindItemDefinition((*FoundItem)->ItemId))
			{
				SlotListItem->DisplayName = Definition->DisplayName;
				SlotListItem->Icon = Definition->Icon;
				SlotListItem->Rarity = Definition->Rarity;
				SlotListItem->ItemType = Definition->ItemType;
				SlotListItem->RequiredLevel = Definition->RequiredLevel;
			}
		}

		SlotListItems.Add(SlotListItem);
		ListItems.Add(SlotListItem);
	}

	InventoryTileView->SetListItems(ListItems);
	RefreshFooter();
	BP_OnInventoryRefreshed();
}

void UTDInventoryContentWidget::HandleInventoryChanged()
{
	// FastArray 삭제 알림 직후에는 삭제 전 항목이 남아 있으므로 다음 틱에 읽는다.
	if (UWorld* World = GetWorld(); World && !World->GetTimerManager().IsTimerActive(RefreshTimer))
	{
		RefreshTimer = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RefreshInventory);
	}
}

void UTDInventoryContentWidget::HandleGoldChanged(int32 NewGold)
{
	SetDisplayedGold(NewGold);
}

void UTDInventoryContentWidget::SetDisplayedGold(int64 InGold)
{
	DisplayedGold = FMath::Max<int64>(0, InGold);
	RefreshFooter();
}

void UTDInventoryContentWidget::SetExternalItems(
	const TArray<FTDInventoryExternalItemView>& InItems,
	int32 InSlotCapacity)
{
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(
			this,
			&ThisClass::HandleInventoryChanged);
		InventoryComponent->OnGoldChanged.RemoveDynamic(this, &ThisClass::HandleGoldChanged);
		InventoryComponent = nullptr;
	}

	bUseExternalItems = true;
	ExternalItems = InItems;
	ExternalSlotCapacity = FMath::Clamp(
		FMath::Max(InSlotCapacity, InItems.Num()),
		1,
		UTDInventoryComponent::MaxSlotCapacity);

	RefreshInventory();
}

bool UTDInventoryContentWidget::HandleExternalSlotClick(
	UTDInventorySlotListItem* Item)
{
	if (!bUseExternalItems
		|| !IsValid(Item)
		|| Item->GetTypedOuter<UTDInventoryContentWidget>() != this)
	{
		return false;
	}

	if (Item->bHasItem && Item->bInteractionEnabled)
	{
		OnExternalSlotClicked.Broadcast(
			Item->SlotIndex,
			Item->ItemInstance.ItemId);
	}

	// 비활성 외부 슬롯도 일반 인벤토리의 사용·장착·드래그로 넘어가면 안 됩니다.
	return true;
}

void UTDInventoryContentWidget::RefreshFooter()
{
	if (bUseExternalItems)
	{
		if (GoldText)
		{
			GoldText->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (CapacityText)
		{
			CapacityText->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (ExpandInventoryButton)
		{
			ExpandInventoryButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (GoldText)
	{
		FNumberFormattingOptions Format;
		Format.SetUseGrouping(true);
		GoldText->SetText(FText::Format(NSLOCTEXT("TDInventory", "Gold", "{0} G"),
			FText::AsNumber(IsDesignTime() ? int64(12480) : DisplayedGold, &Format)));
	}
	if (CapacityText)
	{
		int32 Used = 0;
		int32 Capacity = 0;
		if (InventoryOverrideTable || (IsDesignTime() && bUsePreviewInventory))
		{
			Capacity = SlotListItems.Num();
			for (const UTDInventorySlotListItem* Item : SlotListItems)
			{
				if (Item && Item->bHasItem) ++Used;
			}
		}
		else if (IsDesignTime())
		{
			Used = 32;
			Capacity = 60;
		}
		else if (IsValid(InventoryComponent))
		{
			Used = InventoryComponent->GetUsedSlotCount();
			Capacity = InventoryComponent->GetSlotCapacity();
		}
		CapacityText->SetText(Capacity > 0
			? FText::Format(NSLOCTEXT("TDInventory", "Capacity", "{0} / {1}"), FText::AsNumber(Used), FText::AsNumber(Capacity))
			: NSLOCTEXT("TDInventory", "PendingCapacity", "-- / --"));
	}
}

void UTDInventoryContentWidget::CheckInventorySource()
{
	if (bUseExternalItems)
	{
		RefreshFooter();
		return;
	}

	UTDInventoryComponent* PreviousInventory = InventoryComponent;
	BindInventoryComponent();
	UpdatePendingItemAction();
	// PlayerState의 늦은 도착과 칸 수만 복제되는 확장도 반영한다.
	if (PreviousInventory != InventoryComponent ||
		(!InventoryOverrideTable && IsValid(InventoryComponent) && SlotListItems.Num() != InventoryComponent->GetSlotCapacity()))
	{
		RefreshInventory();
	}
	else
	{
		RefreshFooter();
	}
}

void UTDInventoryContentWidget::HandleExpandClicked()
{
	OnExpansionRequested.Broadcast();
}

void UTDInventoryContentWidget::BuildPreviewInventory()
{
	BuildInventoryFromTable(PreviewItemTable);
	BP_OnInventoryRefreshed();
}

void UTDInventoryContentWidget::BuildInventoryFromTable(UDataTable* SourceTable)
{
	if (!IsValid(InventoryTileView) || !IsValid(SourceTable))
	{
		return;
	}

	SlotListItems.Reset();

	TArray<FName> ItemIds = SourceTable->GetRowNames();
	ItemIds.Sort(FNameLexicalLess());

	const int32 Capacity = FMath::Clamp(
		FMath::Max(PreviewSlotCapacity, ItemIds.Num()),
		1,
		UTDInventoryComponent::MaxSlotCapacity);

	TArray<UObject*> ListItems;
	ListItems.Reserve(Capacity);
	SlotListItems.Reserve(Capacity);

	for (int32 SlotIndex = 0; SlotIndex < Capacity; ++SlotIndex)
	{
		UTDInventorySlotListItem* SlotListItem = NewObject<UTDInventorySlotListItem>(this);
		SlotListItem->SlotIndex = SlotIndex;

		if (ItemIds.IsValidIndex(SlotIndex))
		{
			SlotListItem->bIsPreviewItem = true;
			SlotListItem->TooltipDefinitionTable = SourceTable;
			const FName ItemId = ItemIds[SlotIndex];
			const FTDItemRow* Definition = SourceTable->FindRow<FTDItemRow>(
				ItemId, TEXT("UTDInventoryContentWidget::BuildInventoryFromTable"), false);

			SlotListItem->bHasItem = true;
			SlotListItem->ItemInstance.ItemId = ItemId;
			SlotListItem->ItemInstance.SlotIndex = SlotIndex;
			SlotListItem->ItemInstance.Count = Definition && Definition->bStackable
				? FMath::Max(1, Definition->MaxStackSize)
				: 1;

			if (Definition)
			{
				SlotListItem->DisplayName = Definition->DisplayName;
				// 테스트 테이블은 최대 200행으로 제한되어 있다. 디자이너와 PIE 모두 같은 결과를
				// 보도록 아이콘을 미리 로드한다. 실제 인벤토리는 기존 비동기 경로를 유지한다.
				Definition->Icon.LoadSynchronous();
				SlotListItem->Icon = Definition->Icon;
				SlotListItem->Rarity = Definition->Rarity;
				SlotListItem->ItemType = Definition->ItemType;
				SlotListItem->RequiredLevel = Definition->RequiredLevel;
			}
		}

		SlotListItems.Add(SlotListItem);
		ListItems.Add(SlotListItem);
	}

	InventoryTileView->SetListItems(ListItems);
}

void UTDInventoryContentWidget::BuildExternalInventory()
{
	if (!IsValid(InventoryTileView))
	{
		return;
	}

	SlotListItems.Reset();

	TMap<int32, const FTDInventoryExternalItemView*> ItemsBySlot;
	for (int32 Index = 0; Index < ExternalItems.Num(); ++Index)
	{
		const FTDInventoryExternalItemView& Item = ExternalItems[Index];
		const int32 DisplaySlot = Item.SlotIndex >= 0 ? Item.SlotIndex : Index;

		if (DisplaySlot >= 0 && DisplaySlot < ExternalSlotCapacity)
		{
			ItemsBySlot.Add(DisplaySlot, &Item);
		}
	}

	TArray<UObject*> ListItems;
	ListItems.Reserve(ExternalSlotCapacity);
	SlotListItems.Reserve(ExternalSlotCapacity);

	for (int32 SlotIndex = 0; SlotIndex < ExternalSlotCapacity; ++SlotIndex)
	{
		UTDInventorySlotListItem* SlotItem =
			NewObject<UTDInventorySlotListItem>(this);
		SlotItem->SlotIndex = SlotIndex;
		SlotItem->bIsPreviewItem = true;

		if (const FTDInventoryExternalItemView* const* Found =
			ItemsBySlot.Find(SlotIndex))
		{
			const FTDInventoryExternalItemView& Source = **Found;
			SlotItem->bHasItem = true;
			SlotItem->ItemInstance.ItemId = Source.ItemId;
			SlotItem->ItemInstance.SlotIndex = Source.SlotIndex;
			SlotItem->ItemInstance.Count = FMath::Max(1, Source.Count);
			SlotItem->DisplayName = Source.DisplayName;
			SlotItem->Icon = Source.Icon;
			SlotItem->Rarity = Source.Rarity;
			SlotItem->InteractionHint = Source.InteractionHint;
			SlotItem->bInteractionEnabled = Source.bInteractionEnabled;
		}

		SlotListItems.Add(SlotItem);
		ListItems.Add(SlotItem);
	}

	InventoryTileView->SetListItems(ListItems);
	RefreshFooter();
	BP_OnInventoryRefreshed();
}
