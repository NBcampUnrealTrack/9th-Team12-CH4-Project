// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDUIRootWidget.generated.h"

class UCanvasPanel;
class APawn;
class UOverlay;
class UVerticalBox;
class UCommonActivatableWidgetStack;
class UTDWindowBaseWidget;
class UTDBossHPWidget;

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
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

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

	UTDBossHPWidget* GetBossWidget() const { return BossHPWidget; }

    void SetLoadingVisible(bool bVisible);

    /** 선택한 캐릭터와 Pawn의 연결을 확인하고 HUD 데이터를 갱신한다. */
    UFUNCTION(BlueprintCallable, Category="TD|UI")
    void RefreshPlayerHUD();
    bool IsPlayerHUDReady() const { return bPlayerHUDReady; }


	/** WindowLayer에 창을 가운데 정렬하여 추가한다. */
	UFUNCTION(BlueprintCallable, Category = "TD|UI")
	bool AddWindow(UTDWindowBaseWidget* WindowWidget);

protected:
	/** WBP_Root의 동일한 이름을 가진 자식 위젯과 연결된다. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTDBossHPWidget> BossHPWidget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Player HUD")
    bool bWaitForCharacterLoad = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Player HUD")
    ESlateVisibility LoadedHUDVisibility = ESlateVisibility::SelfHitTestInvisible;

private:
    FTimerHandle HUDReadyTimer;
    TWeakObjectPtr<APawn> DisplayedPawn;
    bool bPlayerHUDReady = false;

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
