#pragma once

#include "CoreMinimal.h"
#include "Items/TDItemTypes.h"

/** UI 전용 정책. 서버 장착 코드/규칙을 변경하지 않는다. */
namespace TDInventoryAction
{
	inline int32 FindEquipSlot(TConstArrayView<FTDItemInstance> Equipped, int32 SlotCount)
	{
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			bool bOccupied = false;
			for (const FTDItemInstance& Item : Equipped)
			{
				if (Item.IsValid() && Item.SlotIndex == Slot) { bOccupied = true; break; }
			}
			if (!bOccupied) return Slot;
		}
		return SlotCount > 0 ? SlotCount - 1 : INDEX_NONE;
	}

	inline bool SameItem(const FTDItemInstance* A, const FTDItemInstance& B)
	{
		return A && A->ItemId == B.ItemId && A->Count == B.Count
			&& A->EnhanceLevel == B.EnhanceLevel && A->OptionRarity == B.OptionRarity
			&& A->Options == B.Options;
	}
}

