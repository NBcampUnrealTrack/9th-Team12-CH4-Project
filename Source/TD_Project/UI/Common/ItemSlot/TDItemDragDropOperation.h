#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "TDItemDragDropOperation.generated.h"

class UTDInventoryComponent;
class UTexture2D;

/** 재활용되는 TileView 항목 대신 소유 인벤토리와 아이템 ID를 전달한다. */
UCLASS()
class TD_PROJECT_API UTDItemDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient)
	TWeakObjectPtr<UTDInventoryComponent> SourceInventory;
	UPROPERTY(Transient)
	FName ItemId;
	void SetDragIcon(UTexture2D* Texture);
};
