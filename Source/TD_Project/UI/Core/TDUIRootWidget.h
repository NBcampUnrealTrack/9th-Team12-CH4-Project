// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDUIRootWidget.generated.h"

class UCanvasPanel;
class UOverlay;
class UVerticalBox;
class UCommonActivatableWidgetStack;
class UTDWindowBaseWidget;

/**
 * 
 */

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDUIRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

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

	/** WindowLayer에 창을 가운데 정렬하여 추가한다. */
	UFUNCTION(BlueprintCallable, Category = "TD|UI")
	bool AddWindow(UTDWindowBaseWidget* WindowWidget);

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
