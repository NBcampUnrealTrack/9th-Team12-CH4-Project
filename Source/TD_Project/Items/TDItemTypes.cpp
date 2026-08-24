#include "Items/TDItemTypes.h"

#include "Items/TDInventoryComponent.h"
#include "Items/TDItemUseComponent.h"

namespace
{
	/**
	 * 복제가 반영된 뒤 소유 컴포넌트에 알린다.
	 *
	 * 인벤토리와 장착 목록이 같은 컨테이너 타입을 쓰기 때문에 여기서 갈라야 한다.
	 * 컨테이너 쪽에 델리게이트를 두는 방법도 있지만, USTRUCT 의 비-UPROPERTY 멤버는
	 * 복사될 때 함께 따라다녀 수명을 추적하기 어려워진다.
	 */
	void NotifyOwnerComponent(UActorComponent* Owner)
	{
		if (UTDInventoryComponent* Inventory = Cast<UTDInventoryComponent>(Owner))
		{
			Inventory->BroadcastInventoryChanged();
		}
		else if (UTDItemUseComponent* ItemUse = Cast<UTDItemUseComponent>(Owner))
		{
			ItemUse->BroadcastEquipmentChanged();
		}
	}
}

// 아래 셋은 클라이언트에서 복제가 반영된 직후에만 불린다.
// 서버 쪽 알림은 조작 함수가 직접 낸다.

void FTDItemInstance::PreReplicatedRemove(const FTDItemContainer& InArray)
{
	NotifyOwnerComponent(InArray.OwnerComponent);
}

void FTDItemInstance::PostReplicatedAdd(const FTDItemContainer& InArray)
{
	NotifyOwnerComponent(InArray.OwnerComponent);
}

void FTDItemInstance::PostReplicatedChange(const FTDItemContainer& InArray)
{
	NotifyOwnerComponent(InArray.OwnerComponent);
}
