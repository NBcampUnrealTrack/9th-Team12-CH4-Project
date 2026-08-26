// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDUIRootWidget.generated.h"

class UCanvasPanel;
class UOverlay;
class UVerticalBox;
class UCommonActivatableWidgetStack;

/**
 * 
 */

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDUIRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCommonActivatableWidgetStack* GetScreenStack() const
	{
		return ScreenStack;
	}
	UCommonActivatableWidgetStack* GetModalStack() const
	{
		return ModalStack;
	}

	UCanvasPanel* GetWindowLayer() const
	{
		return WindowLayer;
	}

	UCanvasPanel* GetTooltipLayer() const
	{
		return TooltipLayer;
	}

	UCanvasPanel* GetHUDLayer() const
	{
		return HUDLayer;
	}

	void SetLoadingVisible(bool bVisible);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UCanvasPanel> HUDLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UCanvasPanel> WindowLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UCommonActivatableWidgetStack> ScreenStack;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UCommonActivatableWidgetStack> ModalStack;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UCanvasPanel> TooltipLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UVerticalBox> NotificationLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UOverlay> LoadingLayer;
};
