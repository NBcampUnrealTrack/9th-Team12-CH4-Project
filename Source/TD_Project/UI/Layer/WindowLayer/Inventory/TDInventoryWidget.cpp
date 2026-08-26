#include "TDInventoryWidget.h"

#include "TDInventorySlotData.h"
#include "Components/TileView.h"
#include "Data/TDItemRow.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Items/TDInventoryComponent.h"
#include "UObject/ConstructorHelpers.h"

UTDInventoryWidget::UTDInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UDataTable> ItemTableFinder(
		TEXT("/Game/Data/DataTables/Item/DT_ItemDefinition.DT_ItemDefinition"));
	VirtualItemTable = ItemTableFinder.Object;
}

void UTDInventoryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		if (IsValid(InventoryOverrideTable))
		{
			BuildInventoryFromTable(InventoryOverrideTable);
			BP_OnInventoryRefreshed();
		}
		else if (bUseVirtualInventory)
		{
			BuildVirtualInventory();
		}
	}
}

void UTDInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindInventoryComponent();
	RefreshInventory();
}

void UTDInventoryWidget::NativeDestruct()
{
	if (IsValid(InventoryComponent))
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}

	InventoryComponent = nullptr;
	SlotItems.Reset();

	Super::NativeDestruct();
}

void UTDInventoryWidget::BindInventoryComponent()
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

void UTDInventoryWidget::SetInventoryOverrideTable(UDataTable* InTable)
{
	if (InventoryOverrideTable == InTable)
	{
		return;
	}

	InventoryOverrideTable = InTable;
	RefreshInventory();
}

void UTDInventoryWidget::RefreshInventory()
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

	SlotItems.Reset();

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
	SlotItems.Reserve(Capacity);

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
		UTDInventorySlotData* SlotData = NewObject<UTDInventorySlotData>(this);
		SlotData->SlotIndex = SlotIndex;

		if (const FTDItemInstance* const* FoundItem = ItemsBySlot.Find(SlotIndex))
		{
			SlotData->bHasItem = true;
			SlotData->ItemInstance = **FoundItem;

			if (const FTDItemRow* Definition = InventoryComponent->FindItemDefinition((*FoundItem)->ItemId))
			{
				SlotData->DisplayName = Definition->DisplayName;
				SlotData->Icon = Definition->Icon;
				SlotData->Rarity = Definition->Rarity;
				SlotData->ItemType = Definition->ItemType;
				SlotData->RequiredLevel = Definition->RequiredLevel;
			}
		}

		SlotItems.Add(SlotData);
		ListItems.Add(SlotData);
	}

	InventoryTileView->SetListItems(ListItems);
	BP_OnInventoryRefreshed();
}

void UTDInventoryWidget::HandleInventoryChanged()
{
	RefreshInventory();
}

void UTDInventoryWidget::BuildVirtualInventory()
{
	BuildInventoryFromTable(VirtualItemTable);
	BP_OnInventoryRefreshed();
}

void UTDInventoryWidget::BuildInventoryFromTable(UDataTable* SourceTable)
{
	if (!IsValid(InventoryTileView) || !IsValid(SourceTable))
	{
		return;
	}

	SlotItems.Reset();

	TArray<FName> ItemIds = SourceTable->GetRowNames();
	ItemIds.Sort(FNameLexicalLess());

	const int32 Capacity = FMath::Clamp(
		FMath::Max(VirtualSlotCapacity, ItemIds.Num()),
		1,
		UTDInventoryComponent::MaxSlotCapacity);

	TArray<UObject*> ListItems;
	ListItems.Reserve(Capacity);
	SlotItems.Reserve(Capacity);

	for (int32 SlotIndex = 0; SlotIndex < Capacity; ++SlotIndex)
	{
		UTDInventorySlotData* SlotData = NewObject<UTDInventorySlotData>(this);
		SlotData->SlotIndex = SlotIndex;

		if (ItemIds.IsValidIndex(SlotIndex))
		{
			const FName ItemId = ItemIds[SlotIndex];
			const FTDItemRow* Definition = SourceTable->FindRow<FTDItemRow>(
				ItemId, TEXT("UTDInventoryWidget::BuildInventoryFromTable"), false);

			SlotData->bHasItem = true;
			SlotData->ItemInstance.ItemId = ItemId;
			SlotData->ItemInstance.SlotIndex = SlotIndex;
			SlotData->ItemInstance.Count = Definition && Definition->bStackable
				? FMath::Max(1, Definition->MaxStackSize)
				: 1;

			if (Definition)
			{
				SlotData->DisplayName = Definition->DisplayName;
				// 테스트 테이블은 최대 200행으로 제한되어 있다. 디자이너와 PIE 모두 같은 결과를
				// 보도록 아이콘을 미리 로드한다. 실제 인벤토리는 기존 비동기 경로를 유지한다.
				Definition->Icon.LoadSynchronous();
				SlotData->Icon = Definition->Icon;
				SlotData->Rarity = Definition->Rarity;
				SlotData->ItemType = Definition->ItemType;
				SlotData->RequiredLevel = Definition->RequiredLevel;
			}
		}

		SlotItems.Add(SlotData);
		ListItems.Add(SlotData);
	}

	InventoryTileView->SetListItems(ListItems);
}
