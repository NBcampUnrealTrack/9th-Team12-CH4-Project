
#include "TDInventorySlotEntryWidget.h"

#include "TDInventorySlotListItem.h"

void UTDInventorySlotEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	SlotListItem = Cast<UTDInventorySlotListItem>(ListItemObject);
	BP_OnSlotListItemSet(SlotListItem);
}

void UTDInventorySlotEntryWidget::NativeOnEntryReleased()
{
	SlotListItem = nullptr;
	IUserObjectListEntry::NativeOnEntryReleased();
}
