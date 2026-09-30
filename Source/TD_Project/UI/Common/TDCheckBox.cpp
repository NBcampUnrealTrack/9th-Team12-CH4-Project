#include "UI/Common/TDCheckBox.h"

#include "UI/Common/TDCheckBoxStyleDA.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"

void UTDCheckBox::SetStyleData(UTDCheckBoxStyleDA* InStyleData)
{
	StyleData = InStyleData;
	RefreshStyle();
}

TSharedRef<SWidget> UTDCheckBox::RebuildWidget()
{
	TSharedRef<SWidget> Result = Super::RebuildWidget();
	RefreshStyle();
	return Result;
}

void UTDCheckBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	RefreshStyle();
}

void UTDCheckBox::RefreshStyle()
{
	if (!StyleData || !MyCheckbox.IsValid())
	{
		return;
	}

	IndicatorStyle = StyleData->Style;
	const float Scale = FMath::Max(StyleScale, 0.01f);
	FVector2D LargestImage = FVector2D::ZeroVector;
	FSlateBrush* Images[] = {
		&IndicatorStyle.UncheckedImage, &IndicatorStyle.UncheckedHoveredImage, &IndicatorStyle.UncheckedPressedImage,
		&IndicatorStyle.CheckedImage, &IndicatorStyle.CheckedHoveredImage, &IndicatorStyle.CheckedPressedImage,
		&IndicatorStyle.UndeterminedImage, &IndicatorStyle.UndeterminedHoveredImage, &IndicatorStyle.UndeterminedPressedImage
	};
	for (FSlateBrush* Image : Images)
	{
		Image->ImageSize = FVector2D(Image->ImageSize) * Scale;
		Image->OutlineSettings.CornerRadii *= Scale;
		Image->OutlineSettings.Width *= Scale;
		LargestImage.X = FMath::Max(LargestImage.X, double(Image->ImageSize.X));
		LargestImage.Y = FMath::Max(LargestImage.Y, double(Image->ImageSize.Y));
	}

	// The native checkbox owns the complete hit region; only its centered SImage paints.
	FCheckBoxStyle HitStyle = IndicatorStyle;
	const FSlateNoResource Empty;
	HitStyle.SetCheckBoxType(ESlateCheckBoxType::ToggleButton);
	HitStyle.SetPadding(FMargin(0.f));
	HitStyle.SetUncheckedImage(Empty).SetUncheckedHoveredImage(Empty).SetUncheckedPressedImage(Empty);
	HitStyle.SetCheckedImage(Empty).SetCheckedHoveredImage(Empty).SetCheckedPressedImage(Empty);
	HitStyle.SetUndeterminedImage(Empty).SetUndeterminedHoveredImage(Empty).SetUndeterminedPressedImage(Empty);
	HitStyle.BackgroundImage = HitStyle.BackgroundHoveredImage = HitStyle.BackgroundPressedImage = Empty;
	HitStyle.ForegroundColor = HitStyle.HoveredForeground = HitStyle.PressedForeground = FLinearColor::White;
	HitStyle.CheckedForeground = HitStyle.CheckedHoveredForeground = HitStyle.CheckedPressedForeground = FLinearColor::White;
	HitStyle.UndeterminedForeground = FLinearColor::White;
	SetWidgetStyle(HitStyle);

	const FVector2D HitSize = StyleData->HitAreaSize * Scale;
	MyCheckbox->SetContent(
		SNew(SBox)
		.WidthOverride(FMath::Max(HitSize.X, LargestImage.X))
		.HeightOverride(FMath::Max(HitSize.Y, LargestImage.Y))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Visibility(EVisibility::HitTestInvisible)
		[
			SNew(SImage).Image_UObject(this, &UTDCheckBox::GetIndicatorBrush)
		]);
}

const FSlateBrush* UTDCheckBox::GetIndicatorBrush() const
{
	const bool bHovered = MyCheckbox.IsValid() && MyCheckbox->IsHovered();
	const bool bPressed = MyCheckbox.IsValid() && MyCheckbox->IsPressed();
	switch (GetCheckedState())
	{
	case ECheckBoxState::Checked:
		return bPressed ? &IndicatorStyle.CheckedPressedImage : bHovered ? &IndicatorStyle.CheckedHoveredImage : &IndicatorStyle.CheckedImage;
	case ECheckBoxState::Undetermined:
		return bPressed ? &IndicatorStyle.UndeterminedPressedImage : bHovered ? &IndicatorStyle.UndeterminedHoveredImage : &IndicatorStyle.UndeterminedImage;
	default:
		return bPressed ? &IndicatorStyle.UncheckedPressedImage : bHovered ? &IndicatorStyle.UncheckedHoveredImage : &IndicatorStyle.UncheckedImage;
	}
}

#if WITH_EDITOR
const FText UTDCheckBox::GetPaletteCategory()
{
	return NSLOCTEXT("TDUI", "PaletteCategory", "TD UI");
}
#endif
