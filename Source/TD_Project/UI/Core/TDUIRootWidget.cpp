// Fill out your copyright notice in the Description page of Project Settings.


#include "TDUIRootWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Engine/LocalPlayer.h"
#include "UI/Core/TDUIManagerSubsystem.h"
#include "UI/Layer/WindowLayer/Common/WindowBase/TDWindowBaseWidget.h"

void UTDUIRootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UTDUIManagerSubsystem* UIManager =
			LocalPlayer->GetSubsystem<UTDUIManagerSubsystem>())
		{
			UIManager->RegisterRoot(this);
		}
	}
}

void UTDUIRootWidget::NativeDestruct()
{
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UTDUIManagerSubsystem* UIManager =
			LocalPlayer->GetSubsystem<UTDUIManagerSubsystem>())
		{
			UIManager->UnregisterRoot(this);
		}
	}

	Super::NativeDestruct();
}

void UTDUIRootWidget::SetLoadingVisible(bool bVisible)
{
	if (!IsValid(LoadingLayer))
	{
		return;
	}

	LoadingLayer->SetVisibility(
		bVisible
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed
	);
}

bool UTDUIRootWidget::AddWindow(UTDWindowBaseWidget* WindowWidget)
{
	if (!IsValid(WindowLayer) || !IsValid(WindowWidget))
	{
		return false;
	}

	UCanvasPanelSlot* WindowSlot = WindowLayer->AddChildToCanvas(WindowWidget);
	if (!WindowSlot)
	{
		return false;
	}

	WindowSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	WindowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	WindowSlot->SetPosition(FVector2D::ZeroVector);
	WindowSlot->SetAutoSize(true);
	return true;
}
