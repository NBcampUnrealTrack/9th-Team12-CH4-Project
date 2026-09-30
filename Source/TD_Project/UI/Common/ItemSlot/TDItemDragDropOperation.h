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

	/**
	 * 출발한 인벤토리 칸. 칸끼리 옮길 때 쓴다.
	 *
	 * 퀵슬롯 등록은 ItemId 만으로 되지만, 칸 이동은 "어느 칸에서 왔는가" 가 있어야 한다 —
	 * 같은 아이템이 여러 칸에 있으면 ID 만으로는 어느 것을 집었는지 알 수 없다.
	 */
	UPROPERTY(Transient)
	int32 SourceSlotIndex = INDEX_NONE;

	void SetDragIcon(UTexture2D* Texture);
};
