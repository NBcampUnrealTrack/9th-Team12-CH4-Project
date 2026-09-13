#pragma once

#include "CoreMinimal.h"
#include "Items/TDItemTypes.h"
#include "TDInventorySlotListItem.generated.h"

class UTexture2D;
class UDataTable;

/** TileView 안쪽 슬롯 이미지가 클릭을 먼저 처리해도 소유 화면에 직접 전달합니다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FTDOnInventorySlotDirectClick, UObject*);

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

	/** 상점처럼 같은 슬롯 UI를 다른 화면에서 사용할 때 툴팁 아래에 붙는 안내입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Inventory")
	FText InteractionHint;

	/** false면 슬롯은 보이지만 상점 판매 같은 외부 클릭 동작은 실행하지 않습니다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Inventory")
	bool bInteractionEnabled = true;

	/** 상점처럼 TileView의 선택 이벤트를 사용하지 않는 화면 전용 클릭 통로입니다. */
	FTDOnInventorySlotDirectClick OnDirectClick;
};
