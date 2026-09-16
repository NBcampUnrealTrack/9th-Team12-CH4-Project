#include "UI/Layer/WindowLayer/Option/TDOptionItemEntryWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Core/TDGameplayTags.h"
#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "TDOptionItemEntryWidget"

FText UTDOptionItemEntryWidget::GetRarityText(FGameplayTag Rarity, FLinearColor& OutColor)
{
	if (Rarity == TDTags::Item_Rarity_Rare)
	{
		OutColor = FLinearColor(0.2f, 0.7f, 1.f);
		return LOCTEXT("Rare", "희귀");
	}

	if (Rarity == TDTags::Item_Rarity_Epic)
	{
		OutColor = FLinearColor(0.75f, 0.4f, 1.f);
		return LOCTEXT("Epic", "영웅");
	}

	if (Rarity == TDTags::Item_Rarity_Legendary)
	{
		OutColor = FLinearColor(1.f, 0.7f, 0.25f);
		return LOCTEXT("Legendary", "전설");
	}

	OutColor = FLinearColor(0.85f, 0.9f, 0.96f);

	return Rarity == TDTags::Item_Rarity_Common
		? LOCTEXT("Common", "일반")
		: LOCTEXT("Unknown", "-");
}

void UTDOptionItemEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BTN_Select)
	{
		BTN_Select->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
	}

	RefreshEntry();
}

void UTDOptionItemEntryWidget::NativeDestruct()
{
	if (BTN_Select)
	{
		BTN_Select->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
	}

	Super::NativeDestruct();
}

void UTDOptionItemEntryWidget::SetupEntry(const FTDOptionItemView& InItem, bool bSelected)
{
	ItemView = InItem;
	bEntrySelected = bSelected;
	RefreshEntry();
}

void UTDOptionItemEntryWidget::RefreshEntry()
{
	if (TXT_ItemName)
	{
		TXT_ItemName->SetText(ItemView.DisplayName);
	}

	if (TXT_OptionRarity)
	{
		FLinearColor RarityColor = FLinearColor::White;
		TXT_OptionRarity->SetText(GetRarityText(ItemView.OptionRarity, RarityColor));
		TXT_OptionRarity->SetColorAndOpacity(FSlateColor(RarityColor));
	}

	if (IMG_ItemIcon)
	{
		UTexture2D* Texture = ItemView.Icon.LoadSynchronous();

		IMG_ItemIcon->SetBrushFromTexture(Texture, false);
		IMG_ItemIcon->SetRenderOpacity(Texture ? 1.0f : 0.0f);
	}

	if (BTN_Select)
	{
		BTN_Select->SetBackgroundColor(
			bEntrySelected
				? FLinearColor(0.65f, 0.65f, 0.65f, 1.0f)
				: FLinearColor::White);
	}
}

void UTDOptionItemEntryWidget::HandleClicked()
{
	OnSelected.Broadcast(ItemView.SlotIndex);
}

#undef LOCTEXT_NAMESPACE
