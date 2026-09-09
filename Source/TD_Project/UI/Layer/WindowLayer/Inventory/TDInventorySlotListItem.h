#pragma once

#include "CoreMinimal.h"
#include "Items/TDItemTypes.h"
#include "TDInventorySlotListItem.generated.h"

class UTexture2D;
class UDataTable;


UCLASS(BlueprintType)
class TD_PROJECT_API UTDInventorySlotListItem : public UObject
{
	GENERATED_BODY()

public:
	/** 미리보기 테이블이 기본 아이템 테이블과 다를 때 이름/설명도 같은 원본을 사용한다. */
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> TooltipDefinitionTable;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Inventory")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Inventory")
	bool bHasItem = false;

	/** 테스트 테이블에서 만든 항목은 실제 아이템으로 드래그할 수 없다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Inventory")
	bool bIsPreviewItem = false;

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
