
#include "TDInventorySlotWidget.h"

#include "TDInventorySlotData.h"

void UTDInventorySlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	SlotData = Cast<UTDInventorySlotData>(ListItemObject);
	BP_OnSlotDataSet(SlotData);
}

void UTDInventorySlotWidget::NativeOnEntryReleased()
{
	SlotData = nullptr;
	IUserObjectListEntry::NativeOnEntryReleased();
}
