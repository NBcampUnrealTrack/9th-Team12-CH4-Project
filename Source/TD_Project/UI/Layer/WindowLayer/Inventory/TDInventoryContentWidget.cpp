#include "TDInventoryContentWidget.h"

#include "TDInventorySlotListItem.h"
#include "Components/TileView.h"
#include "Data/TDItemRow.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Items/TDInventoryComponent.h"
#include "UObject/ConstructorHelpers.h"

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
}

void UTDInventoryContentWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindInventoryComponent();
	RefreshInventory();
}

void UTDInventoryContentWidget::NativeDestruct()
{
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}

	InventoryComponent = nullptr;
	SlotListItems.Reset();

	Super::NativeDestruct();
}

void UTDInventoryContentWidget::BindInventoryComponent()
{
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
	}

	InventoryComponent = NewInventory;
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);
	}
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
	// WBP에서 테스트 테이블을 지정한 경우에만 실제 플레이어 인벤토리를 대체한다.
	if (IsValid(InventoryOverrideTable))
	{
		BuildInventoryFromTable(InventoryOverrideTable);
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
	BP_OnInventoryRefreshed();
}

void UTDInventoryContentWidget::HandleInventoryChanged()
{
	RefreshInventory();
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
