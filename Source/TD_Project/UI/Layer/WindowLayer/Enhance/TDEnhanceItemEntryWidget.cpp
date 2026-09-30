#include "UI/Layer/WindowLayer/Enhance/TDEnhanceItemEntryWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UTDEnhanceItemEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BTN_Select)
	{
		BTN_Select->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleClicked);
	}

	RefreshEntry();
}

void UTDEnhanceItemEntryWidget::NativeDestruct()
{
	if (BTN_Select)
	{
		BTN_Select->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleClicked);
	}

	Super::NativeDestruct();
}

void UTDEnhanceItemEntryWidget::SetupEntry(
	const FTDEnhanceItemView& InItem,
	bool bSelected)
{
	ItemView = InItem;
	bEntrySelected = bSelected;
	RefreshEntry();
}

void UTDEnhanceItemEntryWidget::RefreshEntry()
{
	if (TXT_ItemName)
	{
		TXT_ItemName->SetText(ItemView.DisplayName);
	}

	if (TXT_EnhanceLevel)
	{
		TXT_EnhanceLevel->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("+%d"),
					ItemView.EnhanceLevel)));
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

void UTDEnhanceItemEntryWidget::HandleClicked()
{
	OnSelected.Broadcast(ItemView.SlotIndex);
}