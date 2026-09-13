
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
		bHasItem ? SlotListItem->ItemInstance.Count : 0, FText::GetEmpty(),
		bHasItem ? SlotListItem->TooltipDefinitionTable.Get() : nullptr,
		bHasItem ? SlotListItem->ItemInstance.EnhanceLevel : 0);
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
	if (Event.GetEffectingButton() == EKeys::RightMouseButton && IsValid(SlotListItem))
	{
		if (UTDInventoryContentWidget* Content = SlotListItem->GetTypedOuter<UTDInventoryContentWidget>())
		{
			Content->RequestItemAction(SlotListItem);
			return FReply::Handled();
		}
	}
	// 상점 구매·판매 슬롯은 TileView의 선택 이벤트를 사용하지 않고
	// 슬롯 객체가 가진 직접 클릭 콜백으로 소유 화면에 전달합니다.
	// 일반 인벤토리 슬롯은 콜백이 비어 있으므로 기존 동작을 그대로 유지합니다.
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton
		&& IsValid(SlotListItem)
		&& SlotListItem->bInteractionEnabled
		&& SlotListItem->OnDirectClick.IsBound())
	{
		SlotListItem->OnDirectClick.Broadcast(SlotListItem);
		return FReply::Handled();
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

		// 상점의 퀘스트 아이템처럼 상호작용이 금지된 슬롯은
		// 공용 시각 위젯에도 비활성 상태를 전달해 회색 처리합니다.
		// 다음 TileView 항목에 이전 슬롯의 비활성 상태가 남지 않도록
		// Entry가 재사용될 때마다 반드시 갱신합니다.
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
