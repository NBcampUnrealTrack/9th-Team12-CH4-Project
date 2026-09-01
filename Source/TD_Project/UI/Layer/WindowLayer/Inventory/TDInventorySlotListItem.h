#pragma once

#include "CoreMinimal.h"
#include "Items/TDItemTypes.h"
#include "TDInventorySlotListItem.generated.h"

class UTexture2D;


UCLASS(BlueprintType)
class TD_PROJECT_API UTDInventorySlotListItem : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "TD|Inventory")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Inventory")
	bool bHasItem = false;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Inventory")
	FTDItemInstance ItemInstance;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Inventory")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Inventory")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Inventory")
	FGameplayTag Rarity;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Inventory")
	FGameplayTag ItemType;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Inventory")
	int32 RequiredLevel = 0;
};   
