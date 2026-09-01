
#include "TDInventorySlotEntryWidget.h"

#include "TDInventorySlotListItem.h"
#include "UI/Common/ItemSlot/TDItemSlotVisualWidget.h"

void UTDInventorySlotEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	SlotListItem = Cast<UTDInventorySlotListItem>(ListItemObject);

	if (SlotVisualWidget)
	{
		if (SlotListItem)
		{
			FTDItemSlotVisualData VisualData;
			VisualData.bHasItem = SlotListItem->bHasItem;
			VisualData.DisplayName = SlotListItem->DisplayName;
			VisualData.Icon = SlotListItem->Icon;
			VisualData.Count = SlotListItem->bHasItem ? SlotListItem->ItemInstance.Count : 0;
			VisualData.Rarity = SlotListItem->Rarity;

			SlotVisualWidget->SetSlotVisualData(VisualData);
		}
		else
		{
			SlotVisualWidget->ClearSlotVisual();
		}
	}

	BP_OnSlotListItemSet(SlotListItem);
}

void UTDInventorySlotEntryWidget::NativeOnEntryReleased()
{
	if (SlotVisualWidget)
	{
		SlotVisualWidget->ClearSlotVisual();
	}

	SlotListItem = nullptr;
	IUserObjectListEntry::NativeOnEntryReleased();
}
