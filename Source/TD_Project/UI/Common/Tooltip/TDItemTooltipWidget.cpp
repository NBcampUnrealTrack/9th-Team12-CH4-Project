#include "TDItemTooltipWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDItemRow.h"
#include "Data/TDItemStatRow.h"
#include "Data/TDItemUseEffectRow.h"
#include "Data/TDStatRow.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Items/TDInventoryComponent.h"
#include "Items/TDItemUseComponent.h"
#include "Items/TDEnhanceStatics.h"
#include "UI/Common/Tooltip/TDTooltipStatics.h"
#include "Player/TDPlayerState.h"
#include "Stats/TDProgressionComponent.h"
#include "UI/Common/Typography/TDTextBlock.h"
#include "UI/Settings/TDUISettings.h"

#define LOCTEXT_NAMESPACE "TDTooltip"

namespace
{
	template<typename T>
	TArray<const T*> SortedRows(UDataTable* Table)
	{
		TArray<const T*> Result;
		if (!Table || !Table->GetRowStruct() || !Table->GetRowStruct()->IsChildOf(T::StaticStruct())) return Result;
		TArray<FName> Names = Table->GetRowNames();
		Names.Sort(FNameLexicalLess());
		for (const FName Name : Names)
		{
			if (const T* Row = Table->FindRow<T>(Name, TEXT("Tooltip"), false)) Result.Add(Row);
		}
		return Result;
	}

	FText Number(float Value, int32 Decimals = 0)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 0;
		Options.MaximumFractionalDigits = FMath::Clamp(Decimals, 0, 4);
		return FText::AsNumber(Value, &Options);
	}

	FText ItemTypeName(FGameplayTag Tag)
	{
		if (Tag == TDTags::Item_Type_Accessory) return LOCTEXT("Accessory", "장신구");
		if (Tag == TDTags::Item_Type_Consumable) return LOCTEXT("Consumable", "소비 아이템");
		if (Tag == TDTags::Item_Type_Quest) return LOCTEXT("Quest", "퀘스트 아이템");
		if (Tag == TDTags::Item_Type_Misc) return LOCTEXT("Misc", "기타");
		return LOCTEXT("Item", "아이템");
	}

	FText RarityName(FGameplayTag Tag, FLinearColor& Color)
	{
		if (Tag == TDTags::Item_Rarity_Rare)
		{
			Color = FLinearColor(0.2f, 0.7f, 1.f);
			return LOCTEXT("Rare", "희귀");
		}
		if (Tag == TDTags::Item_Rarity_Epic)
		{
			Color = FLinearColor(0.75f, 0.4f, 1.f);
			return LOCTEXT("Epic", "영웅");
		}
		if (Tag == TDTags::Item_Rarity_Legendary)
		{
			Color = FLinearColor(1.f, 0.7f, 0.25f);
			return LOCTEXT("Legendary", "전설");
		}
		Color = FLinearColor(0.85f, 0.9f, 0.96f);
		return Tag == TDTags::Item_Rarity_Common ? LOCTEXT("Common", "일반") : FText::GetEmpty();
	}

	void SetTooltipTextColor(UTextBlock* Widget, const FLinearColor& Color)
	{
		if (UTDTextBlock* TDText = Cast<UTDTextBlock>(Widget))
			TDText->SetTypographyColorOverride(Color);
		else if (Widget)
			Widget->SetColorAndOpacity(Color);
	}

	void ShowText(UTextBlock* Widget, const FText& Text)
	{
		if (!Widget) return;
		Widget->SetText(Text);
		Widget->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UTDItemTooltipWidget::SetTooltipData(const FTDTooltipData& InData)
{
	// 스킬 등 외부에서 완성한 표시 데이터는 아이템 조회로 덮어쓰지 않는다.
	SourceItemId = NAME_None;
	SourceDefinitionTable = nullptr;
	Data = InData;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshContent();
}

bool UTDItemTooltipWidget::SetItem(FName ItemId, int32 Count, FText Hint, UDataTable* DefinitionTable, int32 EnhanceLevel)
{
	SourceEnhanceLevel = FMath::Max(0, EnhanceLevel);
	SourceItemId = ItemId;
	SourceCount = FMath::Max(0, Count);
	SourceHint = Hint;
	SourceDefinitionTable = DefinitionTable;
	return RefreshItem();
}

bool UTDItemTooltipWidget::RefreshItem()
{
	Data = FTDTooltipData();
	if (SourceItemId.IsNone())
	{
		RefreshContent();
		return false;
	}
	const UTDUISettings* Settings = GetDefault<UTDUISettings>();
	const APlayerController* Controller = GetOwningPlayer();
	const ATDPlayerState* State = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;
	const UTDInventoryComponent* Inventory = State ? State->GetInventoryComponent() : nullptr;

	// 실제 소지품은 인벤토리가 사용하는 정의가 우선이다. 테스트 테이블은 명시적 Override.
	const FTDItemRow* Definition = !SourceDefinitionTable && Inventory
		? Inventory->FindItemDefinition(SourceItemId) : nullptr;
	UDataTable* Table = SourceDefinitionTable;
	if (!Definition && !Table) Table = Settings->TooltipItemTable.LoadSynchronous();
	if (!Definition && Table && Table->GetRowStruct() == FTDItemRow::StaticStruct())
		Definition = Table->FindRow<FTDItemRow>(SourceItemId, TEXT("Tooltip"), false);
	if (!Definition)
	{
		RefreshContent();
		return false;
	}

	Data.Title = Definition->DisplayName;
	Data.Icon = Definition->Icon;
	Data.Description = Definition->Description;
	const FText Type = ItemTypeName(Definition->ItemType);
	const FText Rarity = RarityName(Definition->Rarity, Data.TitleColor);
	Data.Subtitle = Rarity.IsEmpty() ? Type : FText::Format(LOCTEXT("Subtitle", "{0} · {1}"), Rarity, Type);
	if (Definition->RequiredLevel > 0)
	{
		Data.Requirement = FText::Format(
			Definition->ItemType == TDTags::Item_Type_Accessory
				? LOCTEXT("EquipLevel", "착용 조건   Lv.{0}") : LOCTEXT("UseLevel", "사용 조건   Lv.{0}"),
			FText::AsNumber(Definition->RequiredLevel));
		const UTDProgressionComponent* Progression = State ? State->GetProgressionComponent() : nullptr;
		Data.bRequirementUnmet = Progression && Progression->GetLevel() < Definition->RequiredLevel;
	}

	// 강화 수치는 같은 종류의 다른 아이템이 아니라 해당 슬롯의 개체에서 전달받는다.
	const bool bAccessory = Definition->ItemType == TDTags::Item_Type_Accessory;
	const int32 Enhance = bAccessory ? SourceEnhanceLevel : 0;
	const UTDItemUseComponent* ItemUse = State ? State->GetItemUseComponent() : nullptr;
	const float Multiplier = bAccessory
		? TDEnhance::GetStatMultiplier(Enhance, Definition->RequiredLevel) : 1.f;
	if (bAccessory)
	{
		FTDTooltipLine Line;
		Line.Label = LOCTEXT("EnhanceLevel", "강화 단계");
		Line.Value = FText::Format(LOCTEXT("EnhanceLevelValue", "+{0}"), FText::AsNumber(Enhance));
		Data.Lines.Add(Line);
	}
	UDataTable* StatTable = bAccessory && !SourceDefinitionTable && ItemUse && ItemUse->GetItemStatTable()
		? ItemUse->GetItemStatTable() : Settings->TooltipItemStatTable.LoadSynchronous();
	const TArray<const FTDStatRow*> Stats = SortedRows<FTDStatRow>(Settings->TooltipStatDefinitionTable.LoadSynchronous());
	for (const FTDItemStatRow* Row : SortedRows<FTDItemStatRow>(StatTable))
	{
		if (Row->ItemId != SourceItemId || !FMath::IsFinite(Row->Value)) continue;
		const FTDStatRow* Meta = nullptr;
		for (const FTDStatRow* Stat : Stats)
		{
			if (Stat->StatTag == Row->StatTag) { Meta = Stat; break; }
		}
		FTDTooltipLine Line;
		Line.Label = Meta && !Meta->DisplayName.IsEmpty() ? Meta->DisplayName : FText::FromString(Row->StatTag.ToString());
		const bool bMultiplier = Row->Op == ETDModOp::Increased || Row->Op == ETDModOp::More;
		const bool bPercent = bMultiplier || (Meta && Meta->bIsPercent);
		const float DisplayValue = Row->Value * Multiplier * (bPercent ? 100.f : 1.f);
		Line.Value = FText::Format(
			bPercent ? LOCTEXT("Percent", "{0}{1}%") : LOCTEXT("Flat", "{0}{1}"),
			DisplayValue > 0 ? FText::FromString(TEXT("+")) : FText::GetEmpty(),
			Number(DisplayValue, Meta ? Meta->DecimalPlaces : 2));
		if (Row->Op == ETDModOp::Increased)
			Line.Label = FText::Format(LOCTEXT("Increased", "{0} (합연산)"), Line.Label);
		else if (Row->Op == ETDModOp::More)
			Line.Label = FText::Format(LOCTEXT("More", "{0} (곱연산)"), Line.Label);
		const float Gain = Row->Value * (Multiplier - 1.f);
		if (Enhance > 0 && !FMath::IsNearlyZero(Gain))
			Line.Value = FText::Format(LOCTEXT("EnhancedStat", "{0} ({1})"), Line.Value,
				UTDTooltipStatics::FormatStatValue(Row->StatTag, Row->Op, Gain));
		if (DisplayValue < 0) Line.ValueColor = FLinearColor(1.f, 0.35f, 0.35f);
		Data.Lines.Add(Line);
	}

	for (const FTDItemUseEffectRow* Row : SortedRows<FTDItemUseEffectRow>(Settings->TooltipUseEffectTable.LoadSynchronous()))
	{
		if (Row->ItemId != SourceItemId || !FMath::IsFinite(Row->Value)) continue;
		FTDTooltipLine Line;
		Line.Value = Number(Row->Value, 2);
		if (Row->EffectTag == TDTags::Item_Effect_RestoreHealth) Line.Label = LOCTEXT("RestoreHealth", "체력 회복");
		else if (Row->EffectTag == TDTags::Item_Effect_RestoreMana) Line.Label = LOCTEXT("RestoreMana", "마나 회복");
		else if (Row->EffectTag == TDTags::Item_Effect_GainExp) Line.Label = LOCTEXT("GainExp", "경험치 획득");
		else if (Row->EffectTag == TDTags::Item_Effect_ExpandInventory)
		{
			Line.Label = LOCTEXT("ExpandInventory", "인벤토리 확장");
			Line.Value = FText::Format(LOCTEXT("Slots", "{0}칸"), Number(Row->Value));
		}
		else if (Row->EffectTag == TDTags::Item_Effect_LevelUp)
		{
			// 이 값은 올려 주는 레벨 수가 아니라 효과 계산의 상한 레벨이다.
			Line.Label = LOCTEXT("LevelTicket", "레벨업권 기준");
			Line.Value = FText::Format(LOCTEXT("LevelCap", "Lv.{0}"), Number(Row->Value));
			const FText Rule = LOCTEXT("LevelTicketRule", "기준 레벨 미만: 다음 레벨까지의 경험치 획득\n기준 레벨 이상: 기준 레벨 한 구간의 경험치 획득");
			Data.Description = Data.Description.IsEmpty() ? Rule :
				FText::Format(LOCTEXT("AppendDescription", "{0}\n{1}"), Data.Description, Rule);
		}
		else Line.Label = FText::FromString(Row->EffectTag.ToString());
		Data.Lines.Add(Line);
	}

	Data.Footer = Definition->bStackable
		? FText::Format(LOCTEXT("Count", "보유 {0}개"), FText::AsNumber(SourceCount)) : Type;
	if (!SourceHint.IsEmpty())
		Data.Footer = FText::Format(LOCTEXT("FooterHint", "{0}\n{1}"), Data.Footer, SourceHint);
	// bCanDiscard는 거래 가능 여부가 아니다. 거래 문구로 표시하지 않는다.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshContent();
	return true;
}

void UTDItemTooltipWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (!IsDesignTime()) RefreshContent();
}

void UTDItemTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!SourceItemId.IsNone()) RefreshItem();
	else RefreshContent();
}

void UTDItemTooltipWidget::RefreshContent()
{
	if (!WidgetTree) return;
	// WBP_ItemTooltip의 장식 영역. 텍스트 카드는 아이콘 공간/중복 구분선 없이 표시한다.
	// 아이콘이 없는 아이템도 기존 아이템 레이아웃은 유지한다.
	const bool bTextOnly = SourceItemId.IsNone() && Data.Icon.IsNull()
		&& Data.Subtitle.IsEmpty() && Data.Requirement.IsEmpty() && Data.Lines.IsEmpty() && Data.Footer.IsEmpty();
	if (UWidget* IconArea = WidgetTree->FindWidget(TEXT("TooltipIconSize")))
		IconArea->SetVisibility(bTextOnly ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (UWidget* Divider = WidgetTree->FindWidget(TEXT("DescriptionDivider")))
		Divider->SetVisibility(bTextOnly ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (UWidget* Header = WidgetTree->FindWidget(TEXT("TooltipHeader")))
		Header->SetVisibility(bTextOnly && Data.Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (UWidget* Divider = WidgetTree->FindWidget(TEXT("HeaderDivider")))
		Divider->SetVisibility(bTextOnly && Data.Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	ShowText(TooltipTitle, Data.Title);
	SetTooltipTextColor(TooltipTitle, Data.TitleColor);
	ShowText(TooltipSubtitle, Data.Subtitle);
	ShowText(TooltipRequirement, Data.Requirement);
	SetTooltipTextColor(TooltipRequirement,
		Data.bRequirementUnmet ? FLinearColor(1.f, 0.35f, 0.35f) : BodyColor);
	ShowText(TooltipDescription, Data.Description);
	ShowText(TooltipFooter, Data.Footer);
	if (TooltipIcon)
	{
		if (!Data.Icon.IsNull()) TooltipIcon->SetBrushFromSoftTexture(Data.Icon);
		else TooltipIcon->SetBrushFromTexture(nullptr);
		TooltipIcon->SetVisibility(Data.Icon.IsNull() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}
	if (!TooltipLines) return;
	TooltipLines->ClearChildren();
	TooltipLines->SetVisibility(Data.Lines.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	for (const FTDTooltipLine& Line : Data.Lines)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		TooltipLines->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 5.f));
		if (!Line.Icon.IsNull())
		{
			USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>();
			IconSize->SetWidthOverride(22.f);
			IconSize->SetHeightOverride(22.f);
			UImage* Icon = WidgetTree->ConstructWidget<UImage>();
			Icon->SetBrushFromSoftTexture(Line.Icon);
			IconSize->SetContent(Icon);
			Row->AddChildToHorizontalBox(IconSize)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		UTDTextBlock* Label = WidgetTree->ConstructWidget<UTDTextBlock>();
		Label->SetTextStyleRole(RowTextStyleRole);
		Label->SetCustomStyleName(RowCustomStyleName);
		Label->SetText(Line.Label);
		Label->SetTypographyColorOverride(BodyColor);
		Label->SetAutoWrapText(true);
		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		UTDTextBlock* Value = WidgetTree->ConstructWidget<UTDTextBlock>();
		Value->SetTextStyleRole(RowTextStyleRole);
		Value->SetCustomStyleName(RowCustomStyleName);
		Value->SetText(Line.Value);
		Value->SetTypographyColorOverride(Line.ValueColor);
		Row->AddChildToHorizontalBox(Value);
	}
}

void UTDItemTooltipWidget::ClearItemTooltip(UUserWidget* Host)
{
	if (!Host) return;
	if (UTDItemTooltipWidget* Old = Cast<UTDItemTooltipWidget>(Host->GetToolTip()))
		Old->SetVisibility(ESlateVisibility::Collapsed);
	Host->SetToolTipText(FText::GetEmpty());
	Host->SetToolTip(nullptr);
}

void UTDItemTooltipWidget::AttachData(UUserWidget* Owner, UWidget* Host, const FTDTooltipData& InData)
{
	if (!Host) return;
	AttachText(Owner, Host, InData.Title, InData.Description.IsEmpty() ? InData.Title : InData.Description);
	if (UTDItemTooltipWidget* Tooltip = Cast<UTDItemTooltipWidget>(Host->GetToolTip()))
		Tooltip->SetTooltipData(InData);
}

void UTDItemTooltipWidget::AttachText(UUserWidget* Owner, UWidget* Host,
	const FText& Title, const FText& Description)
{
	if (!Owner || !Host || Owner->IsDesignTime() || !Owner->GetWorld()
		|| Owner->GetWorld()->GetNetMode() == NM_DedicatedServer) return;
	if (Description.IsEmpty())
	{
		Host->SetToolTipText(FText::GetEmpty());
		Host->SetToolTip(nullptr);
		return;
	}
	UTDItemTooltipWidget* Tooltip = Cast<UTDItemTooltipWidget>(Host->GetToolTip());
	if (!Tooltip)
	{
		UClass* Class = GetDefault<UTDUISettings>()->ItemTooltipClass.LoadSynchronous();
		if (Class && Class->IsChildOf(StaticClass()))
			Tooltip = Owner->GetOwningPlayer() ? CreateWidget<UTDItemTooltipWidget>(Owner->GetOwningPlayer(), Class)
				: CreateWidget<UTDItemTooltipWidget>(Owner->GetWorld(), Class);
	}
	if (!Tooltip)
	{
		Host->SetToolTip(nullptr);
		Host->SetToolTipText(Description);
		return;
	}
	FTDTooltipData TextData;
	TextData.Title = Title;
	TextData.Description = Description;
	Tooltip->SetTooltipData(TextData);
	// 디자이너의 원문은 보존한다. 재구성 시에도 읽을 수 있고 커스텀 카드가 우선한다.
	Host->SetToolTip(Tooltip);
}

void UTDItemTooltipWidget::AttachItem(UUserWidget* Host, FName ItemId, int32 Count,
	const FText& Hint, UDataTable* DefinitionTable, int32 EnhanceLevel)
{
	if (!Host || Host->IsDesignTime()) return;
	if (ItemId.IsNone()) { ClearItemTooltip(Host); return; }
	if (!Host->GetWorld() || Host->GetWorld()->GetNetMode() == NM_DedicatedServer) return;
	UTDItemTooltipWidget* Tooltip = Cast<UTDItemTooltipWidget>(Host->GetToolTip());
	const bool bNew = !Tooltip;
	if (!Tooltip)
	{
		UClass* Class = GetDefault<UTDUISettings>()->ItemTooltipClass.LoadSynchronous();
		if (!Class || !Class->IsChildOf(StaticClass()))
		{
			ClearItemTooltip(Host);
			return;
		}
		Tooltip = Host->GetOwningPlayer() ? CreateWidget<UTDItemTooltipWidget>(Host->GetOwningPlayer(), Class)
			: CreateWidget<UTDItemTooltipWidget>(Host->GetWorld(), Class);
	}
	if (!Tooltip || !Tooltip->SetItem(ItemId, Count, Hint, DefinitionTable, EnhanceLevel))
	{
		ClearItemTooltip(Host);
		return;
	}
	if (bNew)
	{
		Host->SetToolTipText(FText::GetEmpty());
		Host->SetToolTip(Tooltip);
	}
}

#undef LOCTEXT_NAMESPACE
