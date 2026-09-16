
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
	// 개체를 통째로 넘긴다 — 강화 단계와 추가 옵션이 함께 따라간다.
	UTDItemTooltipWidget::AttachItemInstance(this,
		bHasItem ? SlotListItem->ItemInstance : FTDItemInstance(),
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
	// 소비 아이템만 끌 수 있던 제한을 풀었다(2026-09-17) — 칸끼리 옮기려면 장비도 집어야 한다.
	// 퀵슬롯 등록은 그쪽(RequestRegisterItem)이 타입을 따로 검사하므로 그대로 거부된다.
	const FTDItemRow* Definition = Inventory->FindItemDefinition(Item->ItemId);
	return Definition != nullptr ? Inventory : nullptr;
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
	Drag->SourceSlotIndex = SlotListItem->SlotIndex;
	Drag->SetDragIcon(SlotListItem->Icon.LoadSynchronous());
	Operation = Drag;
}

// 드롭은 이 엔트리가 아니라 목록을 들고 있는 UTDInventoryContentWidget 이 받는다.
// TileView 의 엔트리는 STableRow 안에 들어가 드롭 이벤트가 오지 않는다 —
// 퀵슬롯도 같은 이유로 부모가 받아 마우스 위치로 칸을 찾는다(2026-09-17).

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
			VisualData.bShowEnhanceLevel = SlotListItem->bHasItem
				&& SlotListItem->ItemType == TDTags::Item_Type_Accessory.GetTag();
			VisualData.EnhanceLevel = VisualData.bShowEnhanceLevel
				? FMath::Max(0, SlotListItem->ItemInstance.EnhanceLevel) : 0;

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
