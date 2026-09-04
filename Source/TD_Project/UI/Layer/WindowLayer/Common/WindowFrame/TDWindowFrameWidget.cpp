// Fill out your copyright notice in the Description page of Project Settings.


#include "TDWindowFrameWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "UI/Common/TDButtonStyleDA.h"

void UTDWindowFrameWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshCloseButtonStyle();
}

void UTDWindowFrameWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleCloseClicked
		);
	}

	RefreshCloseButtonStyle();
}

void UTDWindowFrameWidget::NativeDestruct()
{
	EndWindowDrag();
	Super::NativeDestruct();
}

FReply UTDWindowFrameWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bEnableDragging || !IsValid(DragHandle)
		|| InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton
		|| !DragHandle->GetCachedGeometry().IsUnderLocation(
			InMouseEvent.GetScreenSpacePosition()))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	UCanvasPanel* Canvas = nullptr;
	UCanvasPanelSlot* CanvasSlot = nullptr;

	if (DragTargetOverride.IsValid())
	{
		CanvasSlot = FindCanvasSlot(DragTargetOverride.Get(), Canvas);
	}
	else
	{
		// A frame placed directly in WindowLayer/HUDLayer already owns the
		// Canvas Panel slot that must move. Prefer it before crossing the
		// nested UserWidget boundary to an outer window.
		CanvasSlot = FindCanvasSlot(this, Canvas);
		if (!IsValid(CanvasSlot))
		{
			CanvasSlot = FindCanvasSlot(ResolveDefaultDragTarget(), Canvas);
		}
	}

	if (!IsValid(CanvasSlot) || !IsValid(Canvas))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	ActiveDragSlot = CanvasSlot;
	ActiveDragCanvas = Canvas;
	LastMousePosition = Canvas->GetCachedGeometry().AbsoluteToLocal(
		InMouseEvent.GetScreenSpacePosition());
	bDragging = true;

	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UTDWindowFrameWidget::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bDragging || !ActiveDragSlot.IsValid() || !ActiveDragCanvas.IsValid())
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	const FVector2D CurrentMousePosition =
		ActiveDragCanvas->GetCachedGeometry().AbsoluteToLocal(
			InMouseEvent.GetScreenSpacePosition());
	const FVector2D DragDelta = CurrentMousePosition - LastMousePosition;

	ActiveDragSlot->SetPosition(ActiveDragSlot->GetPosition() + DragDelta);
	LastMousePosition = CurrentMousePosition;

	return FReply::Handled();
}

FReply UTDWindowFrameWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		EndWindowDrag();
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UTDWindowFrameWidget::NativeOnMouseCaptureLost(
	const FCaptureLostEvent& CaptureLostEvent)
{
	EndWindowDrag();
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UTDWindowFrameWidget::SetTitle(const FText& Title)
{
	if (TitleText)
	{
		TitleText->SetText(Title);
	}
}

void UTDWindowFrameWidget::SetDragTarget(UWidget* InDragTarget)
{
	DragTargetOverride = InDragTarget;
}

void UTDWindowFrameWidget::SetDraggingEnabled(bool bEnabled)
{
	bEnableDragging = bEnabled;

	if (!bEnableDragging)
	{
		EndWindowDrag();
	}
}

void UTDWindowFrameWidget::CloseWindow()
{
	RemoveFromParent();
}

void UTDWindowFrameWidget::HandleCloseClicked()
{
	// WindowBase가 연결되어 있으면 바깥 창이 닫기 동작을 처리한다.
	if (OnCloseRequested.IsBound())
	{
		OnCloseRequested.Broadcast();
		return;
	}

	// 프레임을 단독으로 사용하는 기존 WBP는 이전 동작을 유지한다.
	CloseWindow();
}

void UTDWindowFrameWidget::RefreshCloseButtonStyle()
{
	if (CloseButton && CloseButtonStyleData)
	{
		CloseButton->SetStyle(CloseButtonStyleData->GetStyle(false));
	}
}

UWidget* UTDWindowFrameWidget::ResolveDefaultDragTarget() const
{
	// Designer-created child UserWidgets are owned by the outer window's WidgetTree.
	// That outer UserWidget is normally the widget inserted into WindowLayer.
	for (UObject* Outer = GetOuter(); Outer; Outer = Outer->GetOuter())
	{
		if (UUserWidget* OuterWindow = Cast<UUserWidget>(Outer))
		{
			return OuterWindow;
		}
	}

	return const_cast<UTDWindowFrameWidget*>(this);
}

UCanvasPanelSlot* UTDWindowFrameWidget::FindCanvasSlot(
	UWidget* StartWidget,
	UCanvasPanel*& OutCanvas) const
{
	OutCanvas = nullptr;

	for (UWidget* Widget = StartWidget; IsValid(Widget); Widget = Widget->GetParent())
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			OutCanvas = Cast<UCanvasPanel>(CanvasSlot->Parent);
			return IsValid(OutCanvas) ? CanvasSlot : nullptr;
		}
	}

	return nullptr;
}

void UTDWindowFrameWidget::EndWindowDrag()
{
	bDragging = false;
	ActiveDragSlot.Reset();
	ActiveDragCanvas.Reset();
	LastMousePosition = FVector2D::ZeroVector;
}
