#include "TDItemSlotVisualWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UI/Common/Tooltip/TDItemTooltipWidget.h"

void UTDItemSlotVisualWidget::SetItemTooltipSource(FName ItemId, FText Hint, int32 EnhanceLevel)
{
	TooltipEnhanceLevel = EnhanceLevel;
	TooltipItemId = ItemId;
	TooltipHint = Hint;
	if (ItemId.IsNone()) UTDItemTooltipWidget::ClearItemTooltip(this);
	RefreshItemTooltip();
}

void UTDItemSlotVisualWidget::RefreshItemTooltip()
{
	if (!TooltipItemId.IsNone())
		UTDItemTooltipWidget::AttachItem(this, SlotVisualData.bHasItem ? TooltipItemId : NAME_None,
			SlotVisualData.Count, TooltipHint, nullptr, TooltipEnhanceLevel);
}

void UTDItemSlotVisualWidget::NativeDestruct()
{
	UTDItemTooltipWidget::ClearItemTooltip(this);
	bSlotHovered = false;
	Super::NativeDestruct();
}

void UTDItemSlotVisualWidget::SetSlotVisualData(const FTDItemSlotVisualData& InData)
{
	SlotVisualData = InData;
	RefreshVisual();
	RefreshItemTooltip();
}

void UTDItemSlotVisualWidget::ClearSlotVisual()
{
	TooltipItemId = NAME_None;
	TooltipHint = FText::GetEmpty();
	UTDItemTooltipWidget::ClearItemTooltip(this);
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
	RefreshItemTooltip();
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
		if (SlotVisualData.bHasItem && (SlotVisualData.Count > 1 || SlotVisualData.bAlwaysShowCount))
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
