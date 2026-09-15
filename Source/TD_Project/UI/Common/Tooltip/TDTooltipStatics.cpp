#include "UI/Common/Tooltip/TDTooltipStatics.h"

#include "Core/TDGameplayTags.h"
#include "Data/TDItemStatRow.h"
#include "Data/TDItemRow.h"
#include "Data/TDItemSetRow.h"
#include "Data/TDOptionRow.h"
#include "Data/TDSkillPassiveRow.h"
#include "Data/TDSkillRow.h"
#include "Data/TDStatRow.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"
#include "Items/TDItemUseComponent.h"
#include "Player/TDPlayerState.h"
#include "Stats/TDProgressionComponent.h"
#include "UI/Settings/TDUISettings.h"

#define LOCTEXT_NAMESPACE "TDTooltipStatics"

namespace
{
	const TCHAR* TooltipContext = TEXT("TDTooltipStatics");

	/** 후행 0 을 붙이지 않는다. 12.00% 가 아니라 12% 로 보여야 한다. */
	FText FormatAmount(float Value, int32 Decimals)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 0;
		Options.MaximumFractionalDigits = FMath::Clamp(Decimals, 0, 4);
		return FText::AsNumber(Value, &Options);
	}

	/**
	 * DT_StatDefinition 에서 그 스탯의 표시 규칙을 찾는다. 없으면 nullptr.
	 *
	 * RowName 이 아니라 StatTag 열로 찾아야 하므로 전체를 훑는다. 툴팁은 마우스를
	 * 올릴 때만 만들어지고 스탯 수가 수십 개라 캐시할 만큼 비싸지 않다.
	 */
	const FTDStatRow* FindStatMeta(FGameplayTag StatTag)
	{
		const UTDUISettings* Settings = GetDefault<UTDUISettings>();
		if (Settings == nullptr || !StatTag.IsValid())
		{
			return nullptr;
		}

		const UDataTable* Table = Settings->TooltipStatDefinitionTable.LoadSynchronous();
		if (Table == nullptr)
		{
			return nullptr;
		}

		TArray<FTDStatRow*> Rows;
		Table->GetAllRows<FTDStatRow>(TooltipContext, Rows);

		for (const FTDStatRow* Row : Rows)
		{
			if (Row != nullptr && Row->StatTag == StatTag)
			{
				return Row;
			}
		}

		return nullptr;
	}

	const UDataTable* FindOptionDefinitionTable(const APlayerController* Owner)
	{
		const ATDPlayerState* State = Owner != nullptr ? Owner->GetPlayerState<ATDPlayerState>() : nullptr;
		const UTDItemUseComponent* ItemUse = State != nullptr ? State->GetItemUseComponent() : nullptr;
		return ItemUse != nullptr ? ItemUse->GetOptionDefinitionTable() : nullptr;
	}

	/**
	 * 태그 하나를 서식 인자 이름 두 가지로 등록한다.
	 *
	 *   Stat.Defense.Armor  →  {Defense_Armor} 와 {Armor}
	 *   Skill.Effect.Damage →  {Damage}          (조각이 하나라 둘이 같다)
	 *
	 * 짧은 이름을 함께 두는 것은 시트를 쓰는 쪽을 위한 것이다. 대부분은 겹치지 않아
	 * {Armor} 로 충분하고, 겹치는 스탯(Health.Max 와 Mana.Max)만 긴 이름을 쓰면 된다.
	 *
	 * 이미 있는 이름은 덮어쓰지 않는다 — 먼저 등록된 쪽이 남는다.
	 */
	void AddTagArgument(FFormatNamedArguments& Args, FGameplayTag Tag, const TCHAR* Prefix, const FText& Value)
	{
		if (!Tag.IsValid())
		{
			return;
		}

		FString Name = Tag.ToString();
		Name.RemoveFromStart(Prefix);

		const FString LongName = Name.Replace(TEXT("."), TEXT("_"));
		if (!Args.Contains(LongName))
		{
			Args.Add(LongName, Value);
		}

		FString ShortName;
		if (Name.Split(TEXT("."), nullptr, &ShortName, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
			&& !Args.Contains(ShortName))
		{
			Args.Add(ShortName, Value);
		}
	}
}

FText UTDTooltipStatics::FormatStatValue(FGameplayTag StatTag, ETDModOp Op, float Value, bool bSigned)
{
	if (!FMath::IsFinite(Value))
	{
		return FText::GetEmpty();
	}

	const FTDStatRow* Meta = FindStatMeta(StatTag);

	// 연산이 곱셈 계열이면 무조건 비율이고, 덧셈이어도 스탯 자체가 비율일 수 있다
	// (치명타 확률은 Added 지만 0~1 이다). 둘 중 하나면 퍼센트로 보여준다.
	const bool bMultiplier = Op == ETDModOp::Increased || Op == ETDModOp::More;
	const bool bPercent = bMultiplier || (Meta != nullptr && Meta->bIsPercent);

	const float DisplayValue = Value * (bPercent ? 100.f : 1.f);

	// 소수는 한 자리까지만 보여준다. 옵션 값은 Min~Max 를 선형으로 굴려 정하므로
	// 5.0638 같은 값이 그대로 들어 있는데, 두 자리까지 내보내면 "+5.06" 이 되어
	// 눈에 거슬리고 옵션끼리 비교하기도 어렵다.
	//
	// DT_StatDefinition 이 0 을 지정한 스탯은 0 으로 둔다 — 정수로 보여 달라는 뜻이다.
	const int32 Decimals = FMath::Min(Meta != nullptr ? Meta->DecimalPlaces : 1, 1);
	const FText Amount = FormatAmount(DisplayValue, Decimals);
	const FText Sign = bSigned && DisplayValue > 0.f ? FText::FromString(TEXT("+")) : FText::GetEmpty();

	return FText::Format(bPercent ? LOCTEXT("Percent", "{0}{1}%") : LOCTEXT("Flat", "{0}{1}"), Sign, Amount);
}

FText UTDTooltipStatics::FormatStatName(FGameplayTag StatTag)
{
	// 세트 효과 줄(MakeItemSetLines)과 같은 규칙이다 — 이름이 비어 있으면 태그를 그대로 보여 준다.
	// 빈 칸으로 두면 "무엇이 오르는지" 가 사라져 데이터 누락을 화면에서 알아챌 수 없다.
	const FTDStatRow* Meta = FindStatMeta(StatTag);

	return Meta != nullptr && !Meta->DisplayName.IsEmpty()
		? Meta->DisplayName
		: FText::FromString(StatTag.ToString());
}

// ── 아이템 추가 옵션 ──────────────────────────────────────

FText UTDTooltipStatics::FormatItemOption(const APlayerController* Owner, const FTDItemOption& Option)
{
	const UDataTable* Table = FindOptionDefinitionTable(Owner);
	if (Table == nullptr || Option.OptionId.IsNone())
	{
		return FText::GetEmpty();
	}

	const FTDOptionDefinitionRow* Row =
		Table->FindRow<FTDOptionDefinitionRow>(Option.OptionId, TooltipContext, false);
	if (Row == nullptr)
	{
		return FText::GetEmpty();
	}

	// 문구에 이미 부호와 `%` 가 들어 있다("최대 체력 +{0}%"). 값만 넣는다.
	return FText::Format(Row->DisplayName,
		FormatStatValue(Row->StatTag, Row->Op, Option.Value, /*bSigned=*/false));
}

TArray<FTDTooltipLine> UTDTooltipStatics::MakeOptionLines(const APlayerController* Owner,
	const TArray<FTDItemOption>& Options)
{
	TArray<FTDTooltipLine> Result;
	Result.Reserve(Options.Num());

	for (const FTDItemOption& Option : Options)
	{
		const FText Text = FormatItemOption(Owner, Option);
		if (Text.IsEmpty())
		{
			continue;
		}

		FTDTooltipLine Line;
		Line.Label = Text;
		Result.Add(Line);
	}

	return Result;
}

// ── 아이템 고정 스탯 ──────────────────────────────────────

TArray<FTDTooltipLine> UTDTooltipStatics::MakeItemStatLines(const APlayerController* Owner,
	FName ItemId, int32 EnhanceLevel)
{
	TArray<FTDTooltipLine> Result;

	const ATDPlayerState* State = Owner != nullptr ? Owner->GetPlayerState<ATDPlayerState>() : nullptr;
	const UTDItemUseComponent* ItemUse = State != nullptr ? State->GetItemUseComponent() : nullptr;
	const UDataTable* Table = ItemUse != nullptr ? ItemUse->GetItemStatTable() : nullptr;

	if (Table == nullptr || ItemId.IsNone())
	{
		return Result;
	}

	// 강화 단수가 0 이면 1.0 이라 아래 계산이 그대로 원래 값이 된다.
	const float Multiplier = ItemUse->GetEnhanceMultiplier(ItemId, EnhanceLevel);

	// RowName 순으로 본다. UTDItemTooltipWidget 이 같은 순서로 그리므로, 줄을 바꿔치기해도
	// 스탯의 위아래가 뒤바뀌지 않는다. GetAllRows 는 맵 순회라 순서를 보장하지 않는다.
	TArray<FName> RowNames = Table->GetRowNames();
	RowNames.Sort(FNameLexicalLess());

	for (const FName& RowName : RowNames)
	{
		const FTDItemStatRow* Row = Table->FindRow<FTDItemStatRow>(RowName, TooltipContext, false);
		if (Row == nullptr || Row->ItemId != ItemId || !FMath::IsFinite(Row->Value))
		{
			continue;
		}

		const FTDStatRow* Meta = FindStatMeta(Row->StatTag);

		FTDTooltipLine Line;
		Line.Label = Meta != nullptr && !Meta->DisplayName.IsEmpty()
			? Meta->DisplayName : FText::FromString(Row->StatTag.ToString());

		// 같은 스탯 태그라도 연산이 다르면 다른 값이다. 아이템 툴팁이 이미 이렇게
		// 구분하고 있어 여기서도 같은 표기를 쓴다 — 두 경로의 화면이 갈리면 안 된다.
		if (Row->Op == ETDModOp::Increased)
		{
			Line.Label = FText::Format(LOCTEXT("Increased", "{0} (합연산)"), Line.Label);
		}
		else if (Row->Op == ETDModOp::More)
		{
			Line.Label = FText::Format(LOCTEXT("More", "{0} (곱연산)"), Line.Label);
		}

		const float Enhanced = Row->Value * Multiplier;
		Line.Value = FormatStatValue(Row->StatTag, Row->Op, Enhanced);

		// 강화로 올라간 몫을 따로 보여준다. 배율만 적으면 "그래서 얼마 오른 건데" 가
		// 남는다 — 아이템마다 배율이 달라서(RequiredLevel) 암산이 되지 않는다.
		const float Gain = Enhanced - Row->Value;
		if (EnhanceLevel > 0 && !FMath::IsNearlyZero(Gain))
		{
			Line.Value = FText::Format(LOCTEXT("EnhanceGain", "{0} ({1})"),
				Line.Value, FormatStatValue(Row->StatTag, Row->Op, Gain));
		}

		if (Enhanced < 0.f)
		{
			Line.ValueColor = FLinearColor(1.f, 0.35f, 0.35f);
		}

		Result.Add(Line);
	}

	return Result;
}

// ── 스킬 ──────────────────────────────────────────────────

FText UTDTooltipStatics::FormatSkillDescription(const UTDProgressionComponent* Progression,
	FName SkillId, int32 Level)
{
	if (Progression == nullptr)
	{
		return FText::GetEmpty();
	}

	const FTDSkillRow* Row = Progression->FindSkillRow(SkillId);
	if (Row == nullptr || Row->Description.IsEmpty())
	{
		return FText::GetEmpty();
	}

	// 아직 찍지 않은 스킬도 "1 을 찍으면 이렇게 된다" 를 보여줘야 찍을지 정할 수 있다.
	const int32 EffectiveLevel = FMath::Max(1, Level);

	FFormatNamedArguments Args;

	// ── 시전 ──
	Args.Add(TEXT("Level"), FText::AsNumber(EffectiveLevel));
	Args.Add(TEXT("Mana"), FormatAmount(Row->ManaCost + Row->ManaCostPerLevel * (EffectiveLevel - 1), 0));
	Args.Add(TEXT("Cooldown"), FormatAmount(FMath::Max(0.f, Row->Cooldown + Row->CooldownPerLevel * (EffectiveLevel - 1)), 1));
	Args.Add(TEXT("CastTime"), FormatAmount(Row->CastDelay, 1));
	Args.Add(TEXT("Duration"), FormatAmount(Row->ChannelDuration, 1));
	Args.Add(TEXT("Interval"), FormatAmount(Row->ChannelInterval, 2));
	Args.Add(TEXT("Range"), FormatAmount(Row->Range, 0));
	Args.Add(TEXT("Width"), FormatAmount(Row->Width, 0));

	// 정신집중이 몇 번 때리는가. 첫 판정이 시작과 동시에 들어가고 지속시간에 끝나므로
	// UTDSkillComponent 의 타이머와 같은 셈이 된다. 정신집중이 아니면 한 번이다.
	const bool bChannel = Row->CastType == ETDSkillCastType::Channel
		&& Row->ChannelDuration > 0.f && Row->ChannelInterval > 0.f;
	const int32 Ticks = bChannel
		? FMath::CeilToInt(Row->ChannelDuration / Row->ChannelInterval - KINDA_SMALL_NUMBER)
		: 1;
	Args.Add(TEXT("Ticks"), FText::AsNumber(Ticks));

	// ── 효과 (액티브) ──
	for (const FTDSkillEffectRow& Effect : Progression->GetSkillEffects(SkillId))
	{
		const float Value = Effect.BaseValue + Effect.ValuePerLevel * (EffectiveLevel - 1);

		// 지속 효과의 시간도 {Duration} 으로 쓴다. 정신집중이 아닌 스킬은 ChannelDuration 이
		// 0 이라 "0초 동안" 이 되어 버리기 때문이다 — 방벽의 무적 1초, 전열 강화의 버프 5초가
		// 그 경우다. 정신집중 스킬은 ChannelDuration 이 이미 맞는 값이라 건드리지 않는다.
		if (Effect.Duration > 0.f && Row->ChannelDuration <= 0.f)
		{
			Args.Add(TEXT("Duration"), FormatAmount(Effect.Duration, 1));
		}

		// 버프는 이름도 값도 **스탯 쪽**을 따라간다.
		//
		// 효과 태그로 이름을 만들면 한 스킬의 버프가 여럿일 때 전부 {Buff} 가 되어
		// 서로 덮어쓴다 — 마법사의 전열 강화가 물리·마법을 따로 올리는 것이 그 경우다.
		// 값도 마찬가지로, 0.3 은 스탯의 표시 규칙을 거쳐야 "30%" 가 된다.
		if (Effect.EffectTag == TDTags::Skill_Effect_Buff && Effect.StatTag.IsValid())
		{
			AddTagArgument(Args, Effect.StatTag, TEXT("Stat."),
				FormatStatValue(Effect.StatTag, Effect.Op, Value, /*bSigned=*/false));
			continue;
		}

		// 피해만 공격력 배율이다(D94). 1.5 를 "150%" 로 내보낸다 — 스탯 태그가 아니라
		// 표시 규칙을 찾을 곳이 없으므로 곱셈 계열이라고 알려 주고 맡긴다.
		// 회복은 절대값이라 숫자만 나간다.
		const FText Text = Effect.EffectTag == TDTags::Skill_Effect_Damage
			? FormatStatValue(FGameplayTag(), ETDModOp::Increased, Value, /*bSigned=*/false)
			: FormatAmount(Value, 0);

		AddTagArgument(Args, Effect.EffectTag, TEXT("Skill.Effect."), Text);
	}

	// ── 효과 (패시브) ──
	for (const FTDSkillPassiveRow& Passive : Progression->GetSkillPassives(SkillId))
	{
		const float Value = Passive.BaseValue + Passive.ValuePerLevel * (EffectiveLevel - 1);

		// 문장이 "{Armor} 오른다" 처럼 부호를 스스로 말하므로 `+` 는 붙이지 않는다.
		AddTagArgument(Args, Passive.StatTag, TEXT("Stat."),
			FormatStatValue(Passive.StatTag, Passive.Op, Value, /*bSigned=*/false));
	}

	return FText::Format(Row->Description, Args);
}

FTDTooltipData UTDTooltipStatics::MakeSkillTooltip(const UTDProgressionComponent* Progression,
	FName SkillId, int32 Level)
{
	FTDTooltipData Data;
	if (Progression == nullptr)
	{
		return Data;
	}

	const FTDSkillRow* Row = Progression->FindSkillRow(SkillId);
	if (Row == nullptr)
	{
		return Data;
	}

	Data.Title = Row->DisplayName;
	Data.Icon = Row->Icon;
	Data.Description = FormatSkillDescription(Progression, SkillId, Level);
	if (Level <= 0) Data.Footer = LOCTEXT("UnlearnedPreview", "미습득 · 1레벨 효과 미리보기");

	// 레벨은 인자를 그대로 쓴다. 설명은 0 을 1 로 보지만 여기서까지 그러면
	// 안 찍은 스킬이 "Lv.1" 로 보여 이미 배운 것처럼 읽힌다.
	const FText Kind = Row->SkillType == ETDSkillType::Passive
		? LOCTEXT("Passive", "패시브") : LOCTEXT("Active", "액티브");
	Data.Subtitle = FText::Format(LOCTEXT("SkillSubtitle", "{0} · Lv.{1} / {2}"),
		Kind, FText::AsNumber(FMath::Max(0, Level)), FText::AsNumber(Row->MaxLevel));

	if (Row->RequiredLevel > 1)
	{
		Data.Requirement = FText::Format(LOCTEXT("SkillRequiredLevel", "습득 조건   Lv.{0}"),
			FText::AsNumber(Row->RequiredLevel));
		Data.bRequirementUnmet = Progression->GetLevel() < Row->RequiredLevel;
	}

	// 패시브는 시전이라는 것이 없어 아래 값이 전부 0 이다. 줄을 만들지 않는다.
	if (Row->SkillType == ETDSkillType::Passive)
	{
		return Data;
	}

	const int32 EffectiveLevel = FMath::Max(1, Level);
	const float Mana = Row->ManaCost + Row->ManaCostPerLevel * (EffectiveLevel - 1);

	auto AddLine = [&Data](const FText& Label, const FText& Value)
	{
		FTDTooltipLine Line;
		Line.Label = Label;
		Line.Value = Value;
		Data.Lines.Add(Line);
	};

	if (Mana > 0.f)
	{
		AddLine(LOCTEXT("SkillMana", "마나 소모"), FormatAmount(Mana, 0));
	}

	if (Row->Cooldown > 0.f || Row->CooldownPerLevel > 0.f)
	{
		AddLine(LOCTEXT("SkillCooldown", "재사용 대기"),
			FText::Format(LOCTEXT("Seconds", "{0}초"), FormatAmount(FMath::Max(0.f, Row->Cooldown + Row->CooldownPerLevel * (EffectiveLevel - 1)), 1)));
	}

	// 범위가 없는 스킬(자기 자신에게 거는 것)은 사거리를 말할 것이 없다.
	if (Row->Range > 0.f)
	{
		AddLine(LOCTEXT("SkillRange", "사거리"), FormatAmount(Row->Range, 0));
	}

	return Data;
}

TArray<FTDTooltipLine> UTDTooltipStatics::MakeItemSetLines(const FTDItemRow& Item,
	const UDataTable* SetTable, const UDataTable* BonusTable, int32 EquippedCount)
{
	TArray<FTDTooltipLine> Lines;
	if (Item.ItemType != TDTags::Item_Type_Accessory || Item.SetId.IsNone()) return Lines;

	const FTDItemSetRow* Set = SetTable && SetTable->GetRowStruct() == FTDItemSetRow::StaticStruct()
		? SetTable->FindRow<FTDItemSetRow>(Item.SetId, TooltipContext, false) : nullptr;
	TArray<FTDItemSetBonusRow> Bonuses;
	if (BonusTable && BonusTable->GetRowStruct() == FTDItemSetBonusRow::StaticStruct())
	{
		TArray<FName> Names = BonusTable->GetRowNames();
		Names.Sort(FNameLexicalLess());
		for (FName Name : Names)
		{
			const FTDItemSetBonusRow* Row = BonusTable->FindRow<FTDItemSetBonusRow>(Name, TooltipContext, false);
			if (Row && Row->SetId == Item.SetId && Row->RequiredCount > 0
				&& Row->StatTag.IsValid() && FMath::IsFinite(Row->Value)) Bonuses.Add(*Row);
		}
	}
	if (!Set && Bonuses.IsEmpty()) return Lines;
	Bonuses.StableSort([](const FTDItemSetBonusRow& A, const FTDItemSetBonusRow& B)
	{
		return A.RequiredCount < B.RequiredCount;
	});

	FTDTooltipLine& Header = Lines.AddDefaulted_GetRef();
	Header.bSectionHeader = true;
	Header.Label = FText::Format(LOCTEXT("SetTitle", "세트 효과 · {0}"),
		Set && !Set->DisplayName.IsEmpty() ? Set->DisplayName : LOCTEXT("UnnamedSet", "세트 장비"));
	Header.Value = FText::Format(LOCTEXT("SetEquipped", "{0}개 착용"), FText::AsNumber(FMath::Max(0, EquippedCount)));
	Header.ValueColor = FLinearColor(1.f, 0.78f, 0.38f);
	for (const FTDItemSetBonusRow& Bonus : Bonuses)
	{
		const bool bActive = EquippedCount >= Bonus.RequiredCount;
		const FTDStatRow* Meta = FindStatMeta(Bonus.StatTag);
		const FText StatName = Meta && !Meta->DisplayName.IsEmpty()
			? Meta->DisplayName : FText::FromString(Bonus.StatTag.ToString());
		FTDTooltipLine& Line = Lines.AddDefaulted_GetRef();
		Line.Label = FText::Format(LOCTEXT("SetBonusLabel", "{0}개 · {1}"), FText::AsNumber(Bonus.RequiredCount), StatName);
		Line.Value = FText::Format(LOCTEXT("SetBonusValue", "{0} ({1})"),
			FormatStatValue(Bonus.StatTag, Bonus.Op, Bonus.Value),
			bActive ? LOCTEXT("SetActive", "활성") : LOCTEXT("SetInactive", "미적용"));
		Line.ValueColor = bActive ? FLinearColor(0.45f, 0.95f, 0.50f) : FLinearColor(0.55f, 0.58f, 0.63f);
	}
	return Lines;
}

#undef LOCTEXT_NAMESPACE
