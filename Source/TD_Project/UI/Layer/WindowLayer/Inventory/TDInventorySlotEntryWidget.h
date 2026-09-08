#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "TDInventorySlotEntryWidget.generated.h"

class UTDInventorySlotListItem;
class UTDItemSlotVisualWidget;
class UTDInventoryComponent;

/** WBP_InventorySlotEntry가 상속할 Tile View Entry 부모. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDInventorySlotEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "TD|Inventory")
	UTDInventorySlotListItem* GetSlotListItem() const { return SlotListItem; }

protected:
	virtual void NativeOnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeDestruct() override;
	void RefreshItemTooltip();

	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& Event, UDragDropOperation*& Operation) override;
	UTDInventoryComponent* GetDraggableInventory() const;

	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnEntryReleased() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "TD|Inventory", meta = (DisplayName = "On Slot List Item Set"))
	void BP_OnSlotListItemSet(UTDInventorySlotListItem* InSlotListItem);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTDItemSlotVisualWidget> SlotVisualWidget;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "TD|Inventory")
	TObjectPtr<UTDInventorySlotListItem> SlotListItem;
};
