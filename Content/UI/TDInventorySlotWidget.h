#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "TDInventorySlotWidget.generated.h"

class UTDInventorySlotData;

/** WBP_InventorySlot이 상속할 Tile View Entry 부모. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDInventorySlotWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "TD|Inventory")
	UTDInventorySlotData* GetSlotData() const { return SlotData; }

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnEntryReleased() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "TD|Inventory", meta = (DisplayName = "On Slot Data Set"))
	void BP_OnSlotDataSet(UTDInventorySlotData* InSlotData);

	UPROPERTY(BlueprintReadOnly, Transient, Category = "TD|Inventory")
	TObjectPtr<UTDInventorySlotData> SlotData;
};
