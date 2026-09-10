#include "Items/TDItemOptionStatics.h"

#include "Engine/DataTable.h"
#include "Settings/TDItemOptionSettings.h"

namespace
{
	const TCHAR* OptionContext = TEXT("TDItemOption");

	/**
	 * 강화(TDEnhance::GetStatPercentPerLevel)가 쓰는 것과 같은 기준이다.
	 *
	 * GetMaxLevel() 을 부르지 않는 이유는 이 함수를 순수하게 두기 위해서다 —
	 * 월드도 컴포넌트도 없이 값을 검증할 수 있어야 한다.
	 * 만렙이 바뀌면 두 곳을 함께 고쳐야 한다.
	 */
	constexpr float ReferenceMaxLevel = 50.f;
}

namespace TDItemOption
{
	TArray<FTDOptionRarityRow> GetSortedRarities(const UDataTable* RarityTable)
	{
		TArray<FTDOptionRarityRow> Result;
		if (RarityTable == nullptr)
		{
			return Result;
		}

		TArray<FTDOptionRarityRow*> Rows;
		RarityTable->GetAllRows<FTDOptionRarityRow>(OptionContext, Rows);

		for (const FTDOptionRarityRow* Row : Rows)
		{
			if (Row != nullptr && Row->Rarity.IsValid())
			{
				Result.Add(*Row);
			}
		}

		// 행 순서를 믿지 않는다. 시트에서 정렬이 흐트러져도 Tier 만 맞으면 동작해야 한다.
		Result.Sort([](const FTDOptionRarityRow& A, const FTDOptionRarityRow& B)
		{
			return A.Tier < B.Tier;
		});

		return Result;
	}

	const FTDOptionRarityRow* FindRarity(const TArray<FTDOptionRarityRow>& Sorted, FGameplayTag Rarity)
	{
		return Sorted.FindByPredicate(
			[Rarity](const FTDOptionRarityRow& Row) { return Row.Rarity == Rarity; });
	}

	FGameplayTag GetNextRarity(const TArray<FTDOptionRarityRow>& Sorted, FGameplayTag Rarity)
	{
		const int32 Index = Sorted.IndexOfByPredicate(
			[Rarity](const FTDOptionRarityRow& Row) { return Row.Rarity == Rarity; });

		// 못 찾았거나 이미 최고 등급이면 그대로 둔다. 부르는 쪽이 "올랐는가" 를
		// 결과 비교로 알 수 있어 별도 분기가 필요 없다.
		if (Index == INDEX_NONE || Index + 1 >= Sorted.Num())
		{
			return Rarity;
		}

		return Sorted[Index + 1].Rarity;
	}

	FGameplayTag GetPreviousRarity(const TArray<FTDOptionRarityRow>& Sorted, FGameplayTag Rarity)
	{
		const int32 Index = Sorted.IndexOfByPredicate(
			[Rarity](const FTDOptionRarityRow& Row) { return Row.Rarity == Rarity; });

		if (Index <= 0)
		{
			return Rarity;
		}

		return Sorted[Index - 1].Rarity;
	}

	int32 GetRerollCost(int32 BaseCost, int32 ItemRequiredLevel)
	{
		if (BaseCost <= 0)
		{
			return 0;
		}

		const UTDItemOptionSettings* Settings = UTDItemOptionSettings::Get();
		const float MaxMultiplier = Settings ? Settings->MaxLevelCostMultiplier : 1.f;

		const float Alpha = FMath::Clamp(
			static_cast<float>(ItemRequiredLevel) / ReferenceMaxLevel, 0.f, 1.f);

		return FMath::RoundToInt(BaseCost * FMath::Lerp(1.f, MaxMultiplier, Alpha));
	}

	FName PickOption(const UDataTable* PoolTable, FName PoolId, FGameplayTag Rarity, float Roll)
	{
		if (PoolTable == nullptr || PoolId.IsNone() || !Rarity.IsValid())
		{
			return NAME_None;
		}

		TArray<FTDOptionPoolRow*> Rows;
		PoolTable->GetAllRows<FTDOptionPoolRow>(OptionContext, Rows);

		// 두 번 훑는다. 가중치 합을 먼저 알아야 어느 지점을 뽑을지 정할 수 있다.
		float TotalWeight = 0.f;
		for (const FTDOptionPoolRow* Row : Rows)
		{
			if (Row != nullptr && Row->PoolId == PoolId && Row->Rarity == Rarity && Row->Weight > 0.f)
			{
				TotalWeight += Row->Weight;
			}
		}

		if (TotalWeight <= 0.f)
		{
			// 그 (풀, 등급) 조합에 행이 없다. 데이터를 아직 안 채운 경우가 대부분이라
			// 여기서 경고를 내면 굴릴 때마다 로그가 쏟아진다. 부르는 쪽이 판단한다.
			return NAME_None;
		}

		float Remaining = FMath::Clamp(Roll, 0.f, 1.f) * TotalWeight;

		for (const FTDOptionPoolRow* Row : Rows)
		{
			if (Row == nullptr || Row->PoolId != PoolId || Row->Rarity != Rarity || Row->Weight <= 0.f)
			{
				continue;
			}

			Remaining -= Row->Weight;
			if (Remaining <= 0.f)
			{
				return Row->OptionId;
			}
		}

		// Roll 이 정확히 1.0 이면 부동소수 오차로 여기까지 올 수 있다. 마지막 것을 준다.
		for (int32 Index = Rows.Num() - 1; Index >= 0; --Index)
		{
			const FTDOptionPoolRow* Row = Rows[Index];
			if (Row != nullptr && Row->PoolId == PoolId && Row->Rarity == Rarity && Row->Weight > 0.f)
			{
				return Row->OptionId;
			}
		}

		return NAME_None;
	}

	float RollOptionValue(const FTDOptionDefinitionRow& Definition, float Roll)
	{
		// 시트에서 상하한이 뒤집혀 들어와도 범위가 깨지지 않게 한다.
		const float Low = FMath::Min(Definition.MinValue, Definition.MaxValue);
		const float High = FMath::Max(Definition.MinValue, Definition.MaxValue);

		return FMath::Lerp(Low, High, FMath::Clamp(Roll, 0.f, 1.f));
	}

	TArray<FTDItemOption> RollLines(const UDataTable* PoolTable, const UDataTable* DefinitionTable,
		const TArray<FTDOptionRarityRow>& Sorted, FName PoolId, FGameplayTag Rarity,
		int32 LineCount, TArrayView<const float> LineRolls)
	{
		TArray<FTDItemOption> Result;

		if (PoolTable == nullptr || DefinitionTable == nullptr || LineCount <= 0)
		{
			return Result;
		}

		const FTDOptionRarityRow* RarityRow = FindRarity(Sorted, Rarity);
		const float SameTierChance = RarityRow ? RarityRow->SameTierLineChance : 1.f;
		const FGameplayTag LowerRarity = GetPreviousRarity(Sorted, Rarity);

		for (int32 Line = 0; Line < LineCount; ++Line)
		{
			const int32 Base = Line * 3;
			if (!LineRolls.IsValidIndex(Base + 2))
			{
				// 주사위가 모자라면 거기서 멈춘다. 부르는 쪽의 실수라 조용히 줄이는 편이
				// 0 으로 채워 넣는 것보다 낫다 — 빈 줄이 눈에 띈다.
				break;
			}

			// 첫 줄은 언제나 현재 등급이다. 등급을 올린 보람이 첫 줄에서 바로 보여야 한다.
			const bool bSameTier = (Line == 0) || (LineRolls[Base] < SameTierChance);
			const FGameplayTag LineRarity = bSameTier ? Rarity : LowerRarity;

			const FName OptionId = PickOption(PoolTable, PoolId, LineRarity, LineRolls[Base + 1]);
			if (OptionId.IsNone())
			{
				continue;
			}

			const FTDOptionDefinitionRow* Definition =
				DefinitionTable->FindRow<FTDOptionDefinitionRow>(OptionId, OptionContext, false);

			if (Definition == nullptr)
			{
				continue;
			}

			FTDItemOption& Option = Result.AddDefaulted_GetRef();
			Option.OptionId = OptionId;
			Option.Value = RollOptionValue(*Definition, LineRolls[Base + 2]);
		}

		return Result;
	}
}
