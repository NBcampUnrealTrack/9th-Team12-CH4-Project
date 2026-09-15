#include "Stats/TDUnionBonus.h"

#include "Data/TDUnionBonusRow.h"
#include "Engine/DataTable.h"

namespace TDUnion
{
	TArray<FTDStatModifier> BuildModifiers(const UDataTable* BonusTable,
		TConstArrayView<FTDCharacterSummary> Characters, int32 CurrentSlot, int32 CurrentLevel)
	{
		TArray<FTDStatModifier> Modifiers;

		if (BonusTable == nullptr)
		{
			return Modifiers;
		}

		// 직업마다 가장 높은 레벨. 같은 직업을 여럿 키워도 한 번만 센다 —
		// 그래야 보상의 최대치가 "세 직업 × 만렙" 으로 고정되고, 한 직업만 여러 번 키우는 쪽이
		// 이득이 되지 않는다.
		TMap<FName, int32> BestLevelByClass;

		for (int32 Slot = 0; Slot < Characters.Num(); ++Slot)
		{
			const FTDCharacterSummary& Character = Characters[Slot];
			if (Character.ClassId.IsNone())
			{
				continue;
			}

			const int32 Level = (Slot == CurrentSlot) ? CurrentLevel : Character.Level;

			int32& Best = BestLevelByClass.FindOrAdd(Character.ClassId, 0);
			Best = FMath::Max(Best, Level);
		}

		BonusTable->ForeachRow<FTDUnionBonusRow>(TEXT("TDUnion::BuildModifiers"),
			[&Modifiers, &BestLevelByClass](const FName&, const FTDUnionBonusRow& Row)
			{
				if (!Row.StatTag.IsValid() || FMath::IsNearlyZero(Row.Value))
				{
					return;
				}

				const int32* Best = BestLevelByClass.Find(Row.ClassId);
				if (Best != nullptr && *Best >= Row.RequiredLevel)
				{
					Modifiers.Emplace(Row.StatTag, Row.Op, Row.Value);
				}
			});

		return Modifiers;
	}
}
