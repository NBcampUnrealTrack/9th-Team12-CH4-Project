#include "TDItemSlotVisualWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

void UTDItemSlotVisualWidget::SetSlotVisualData(const FTDItemSlotVisualData& InData)
{
	SlotVisualData = InData;
	RefreshVisual();
}

void UTDItemSlotVisualWidget::ClearSlotVisual()
{
	SlotVisualData = FTDItemSlotVisualData();
	RefreshVisual();
}

void UTDItemSlotVisualWidget::SetSlotSelected(bool bInSelected)
{
	if (bSlotSelected == bInSelected)
	{
		return;
	}

	bSlotSelected = bInSelected;
	RefreshVisual();
}

void UTDItemSlotVisualWidget::SetSlotEnabled(bool bInEnabled)
{
	if (bSlotEnabled == bInEnabled)
	{
		return;
	}

	bSlotEnabled = bInEnabled;
	if (!bSlotEnabled)
	{
		bSlotHovered = false;
	}
	SetIsEnabled(bSlotEnabled);
	RefreshVisual();
}

void UTDItemSlotVisualWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	SetIsEnabled(bSlotEnabled);
	RefreshVisual();
}

void UTDItemSlotVisualWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (!bSlotEnabled || bSlotHovered)
	{
		return;
	}

	bSlotHovered = true;
	RefreshInteractionVisual();
}

void UTDItemSlotVisualWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	if (!bSlotHovered)
	{
		return;
	}

	bSlotHovered = false;
	RefreshInteractionVisual();
}

void UTDItemSlotVisualWidget::RefreshVisual()
{
	if (ItemIcon)
	{
		if (SlotVisualData.bHasItem && !SlotVisualData.Icon.IsNull())
		{
			ItemIcon->SetBrushFromSoftTexture(SlotVisualData.Icon);
			ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ItemIcon->SetBrushFromTexture(nullptr);
			ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (CountText)
	{
		if (SlotVisualData.bHasItem && SlotVisualData.Count > 1)
		{
			CountText->SetText(FText::AsNumber(SlotVisualData.Count));
			CountText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			CountText->SetText(FText::GetEmpty());
			CountText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	BP_OnSlotVisualStateChanged(SlotVisualData, bSlotSelected, bSlotEnabled);
	RefreshInteractionVisual();
}

void UTDItemSlotVisualWidget::RefreshInteractionVisual()
{
	const bool bShowGlow = bSlotEnabled && (bSlotSelected || bSlotHovered);
	const float OuterOpacity = bSlotSelected ? 0.65f : 0.30f;
	const float MiddleOpacity = bSlotSelected ? 0.90f : 0.55f;
	const float FrameOpacity = bSlotSelected ? 1.0f : 0.45f;

	auto ApplyLayerState = [bShowGlow](UWidget* Layer, const float VisibleOpacity)
	{
		if (!Layer)
		{
			return;
		}

		Layer->SetVisibility(bShowGlow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Layer->SetRenderOpacity(bShowGlow ? VisibleOpacity : 0.0f);
	};

	ApplyLayerState(GlowOuter, OuterOpacity);
	ApplyLayerState(GlowMiddle, MiddleOpacity);
	ApplyLayerState(SelectionFrame, FrameOpacity);
}
