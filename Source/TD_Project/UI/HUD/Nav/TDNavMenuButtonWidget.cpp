#include "TDNavMenuButtonWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/Common/TDButtonStyleDA.h"

void UTDNavMenuButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshContent();
	RefreshStyle();
}

void UTDNavMenuButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button)
	{
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
	}

	RefreshContent();
	RefreshStyle();
}

void UTDNavMenuButtonWidget::NativeDestruct()
{
	if (Button)
	{
		Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
	}

	Super::NativeDestruct();
}

void UTDNavMenuButtonWidget::SetSelected(bool bInSelected)
{
	if (bSelected == bInSelected)
	{
		return;
	}

	bSelected = bInSelected;
	RefreshStyle();
}

void UTDNavMenuButtonWidget::SetStyleData(UTDButtonStyleDA* InStyleData)
{
	StyleData = InStyleData;
	RefreshStyle();
}

void UTDNavMenuButtonWidget::SetMenuType(ETDNavMenuType InMenuType)
{
	MenuType = InMenuType;
}

void UTDNavMenuButtonWidget::RefreshContent()
{
	if (Image)
	{
		Image->SetBrushFromTexture(Icon, false);
	}

	if (Text)
	{
		Text->SetText(Label);
	}
}

void UTDNavMenuButtonWidget::RefreshStyle()
{
	if (Button && StyleData)
	{
		Button->SetStyle(StyleData->GetStyle(bSelected));
	}
}

void UTDNavMenuButtonWidget::HandleClicked()
{
	OnClicked.Broadcast(this, MenuType);
}
