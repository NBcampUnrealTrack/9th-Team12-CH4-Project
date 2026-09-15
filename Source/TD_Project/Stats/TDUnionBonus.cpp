#include "Stats/TDUnionBonus.h"

#include "Data/TDUnionBonusRow.h"
#include "Engine/DataTable.h"

namespace TDUnion
{
	TMap<FName, int32> GetBestLevels(TConstArrayView<FTDCharacterSummary> Characters,
		int32 CurrentSlot, int32 CurrentLevel)
	{
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

		return BestLevelByClass;
	}

	TArray<FTDUnionEntry> BuildEntries(const UDataTable* BonusTable,
		TConstArrayView<FTDCharacterSummary> Characters, int32 CurrentSlot, int32 CurrentLevel)
	{
		TArray<FTDUnionEntry> Entries;

		if (BonusTable == nullptr)
		{
			return Entries;
		}

		const TMap<FName, int32> BestLevelByClass = GetBestLevels(Characters, CurrentSlot, CurrentLevel);

		BonusTable->ForeachRow<FTDUnionBonusRow>(TEXT("TDUnion::BuildEntries"),
			[&Entries, &BestLevelByClass](const FName&, const FTDUnionBonusRow& Row)
			{
				if (!Row.StatTag.IsValid() || FMath::IsNearlyZero(Row.Value))
				{
					return;
				}

				const int32* Best = BestLevelByClass.Find(Row.ClassId);

				FTDUnionEntry& Entry = Entries.AddDefaulted_GetRef();
				Entry.ClassId = Row.ClassId;
				Entry.RequiredLevel = Row.RequiredLevel;
				Entry.StatTag = Row.StatTag;
				Entry.Op = Row.Op;
				Entry.Value = Row.Value;
				Entry.bActive = Best != nullptr && *Best >= Row.RequiredLevel;
			});

		return Entries;
	}

	TArray<FTDStatModifier> BuildModifiers(const UDataTable* BonusTable,
		TConstArrayView<FTDCharacterSummary> Characters, int32 CurrentSlot, int32 CurrentLevel)
	{
		TArray<FTDStatModifier> Modifiers;

		for (const FTDUnionEntry& Entry : BuildEntries(BonusTable, Characters, CurrentSlot, CurrentLevel))
		{
			if (Entry.bActive)
			{
				Modifiers.Emplace(Entry.StatTag, Entry.Op, Entry.Value);
			}
		}

		return Modifiers;
	}
}
