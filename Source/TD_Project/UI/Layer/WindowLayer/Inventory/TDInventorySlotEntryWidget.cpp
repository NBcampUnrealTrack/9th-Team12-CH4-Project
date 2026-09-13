
#include "TDInventorySlotEntryWidget.h"

#include "TDInventorySlotListItem.h"
#include "TDInventoryContentWidget.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDItemRow.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Items/TDInventoryComponent.h"
#include "UI/Common/ItemSlot/TDItemDragDropOperation.h"
#include "UI/Common/ItemSlot/TDItemSlotVisualWidget.h"
#include "UI/Common/Tooltip/TDItemTooltipWidget.h"

void UTDInventorySlotEntryWidget::RefreshItemTooltip()
{
	const bool bHasItem = IsValid(SlotListItem) && SlotListItem->bHasItem;
	UTDItemTooltipWidget::AttachItem(this,
		bHasItem ? SlotListItem->ItemInstance.ItemId : NAME_None,
		bHasItem ? SlotListItem->ItemInstance.Count : 0,
		bHasItem ? SlotListItem->InteractionHint : FText::GetEmpty(),
		bHasItem ? SlotListItem->TooltipDefinitionTable.Get() : nullptr);
}

void UTDInventorySlotEntryWidget::NativeOnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event)
{
	Super::NativeOnMouseEnter(Geometry, Event);
	RefreshItemTooltip();
}

void UTDInventorySlotEntryWidget::NativeDestruct()
{
	UTDItemTooltipWidget::ClearItemTooltip(this);
	Super::NativeDestruct();
}

UTDInventoryComponent* UTDInventorySlotEntryWidget::GetDraggableInventory() const
{
	if (!IsValid(SlotListItem) || !SlotListItem->bHasItem || SlotListItem->bIsPreviewItem) return nullptr;
	const APlayerController* Controller = GetOwningPlayer();
	const APlayerState* State = Controller ? Controller->PlayerState.Get() : nullptr;
	UTDInventoryComponent* Inventory = State ? State->FindComponentByClass<UTDInventoryComponent>() : nullptr;
	const FTDItemInstance* Item = Inventory ? Inventory->FindBySlot(SlotListItem->SlotIndex) : nullptr;
	if (!Item || !Item->IsValid() || Item->ItemId != SlotListItem->ItemInstance.ItemId) return nullptr;
	const FTDItemRow* Definition = Inventory->FindItemDefinition(Item->ItemId);
	return Definition && Definition->ItemType == TDTags::Item_Type_Consumable.GetTag() ? Inventory : nullptr;
}

FReply UTDInventorySlotEntryWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	// WBP_InventoryEntry 안쪽의 슬롯 이미지가 마우스를 먼저 잡으면 TileView의
	// OnItemClicked가 호출되지 않을 수 있다. 상점이 직접 콜백을 등록한 항목만
	// 이 경로로 먼저 전달하며, 일반 인벤토리 슬롯 동작에는 영향을 주지 않는다.
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton
		&& IsValid(SlotListItem)
		&& SlotListItem->bHasItem
		&& SlotListItem->bInteractionEnabled
		&& SlotListItem->OnDirectClick.IsBound())
	{
		SlotListItem->OnDirectClick.Broadcast(SlotListItem);
		return FReply::Handled();
	}

	if (Event.GetEffectingButton() == EKeys::RightMouseButton && IsValid(SlotListItem))
	{
		if (UTDInventoryContentWidget* Content = SlotListItem->GetTypedOuter<UTDInventoryContentWidget>())
		{
			Content->RequestItemAction(SlotListItem);
			return FReply::Handled();
		}
	}
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && IsValid(SlotListItem))
	{
		if (UTDInventoryContentWidget* Content =
			SlotListItem->GetTypedOuter<UTDInventoryContentWidget>())
		{
			if (Content->HandleExternalSlotClick(SlotListItem))
			{
				return FReply::Handled();
			}
		}
	}
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && GetDraggableInventory())
	{
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}
	return Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
}

void UTDInventorySlotEntryWidget::NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation)
{
	UTDInventoryComponent* Inventory = GetDraggableInventory();
	if (!Inventory)
	{
		Super::NativeOnDragDetected(Geometry, Event, Operation);
		return;
	}
	UTDItemDragDropOperation* Drag = NewObject<UTDItemDragDropOperation>(this);
	UTDItemTooltipWidget::ClearItemTooltip(this);
	Drag->SourceInventory = Inventory;
	Drag->ItemId = SlotListItem->ItemInstance.ItemId;
	Drag->SetDragIcon(SlotListItem->Icon.LoadSynchronous());
	Operation = Drag;
}

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

		SlotVisualWidget->SetSlotEnabled(
			!SlotListItem || SlotListItem->bInteractionEnabled);
	}

	BP_OnSlotListItemSet(SlotListItem);
	RefreshItemTooltip();
}

void UTDInventorySlotEntryWidget::NativeOnEntryReleased()
{
	UTDItemTooltipWidget::ClearItemTooltip(this);
	if (SlotVisualWidget)
	{
		SlotVisualWidget->ClearSlotVisual();
		SlotVisualWidget->SetSlotEnabled(true);
	}

	SlotListItem = nullptr;
	IUserObjectListEntry::NativeOnEntryReleased();
}
