// Fill out your copyright notice in the Description page of Project Settings.


#include "TDUIRootWidget.h"
#include "Components/Overlay.h"

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
