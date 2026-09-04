#include "TDInventoryContentWidget.h"

#include "TDInventorySlotListItem.h"
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

void UTDInventoryContentWidget::SetDisplayedGold(int64 InGold)
{
	DisplayedGold = FMath::Max<int64>(0, InGold);
	RefreshFooter();
}

void UTDInventoryContentWidget::RefreshFooter()
{
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
	UTDInventoryComponent* PreviousInventory = InventoryComponent;
	BindInventoryComponent();
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
