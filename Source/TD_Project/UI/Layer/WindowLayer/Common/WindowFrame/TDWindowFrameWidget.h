// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDWindowFrameWidget.generated.h"

class UButton;
class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UNamedSlot;
class UTDButtonStyleDA;
class UTextBlock;
class UWidget;

/**
 * 
 */
UCLASS()
class TD_PROJECT_API UTDWindowFrameWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void SetTitle(const FText& Title);
	UFUNCTION(BlueprintCallable)
	void CloseWindow();

	/**
	 * Sets the widget whose Canvas Panel slot is moved while dragging the title bar.
	 * If unset, the frame automatically finds its outer window and the nearest Canvas Panel slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Window Frame|Dragging")
	void SetDragTarget(UWidget* InDragTarget);
	
protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	
	UPROPERTY(BlueprintReadOnly,meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(BlueprintReadOnly,meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	/** Transparent Border covering only the title bar. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DragHandle;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UNamedSlot> ContentArea;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Window Frame|Dragging")
	bool bEnableDragging = true;

	/** Uses its own asset instance; it does not share images with HUD navigation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Window Frame|Button Style")
	TObjectPtr<UTDButtonStyleDA> CloseButtonStyleData;

private:
	UFUNCTION()
	void HandleCloseClicked();

	void RefreshCloseButtonStyle();
	UWidget* ResolveDefaultDragTarget() const;
	UCanvasPanelSlot* FindCanvasSlot(
		UWidget* StartWidget,
		UCanvasPanel*& OutCanvas) const;
	void EndWindowDrag();

	TWeakObjectPtr<UWidget> DragTargetOverride;
	TWeakObjectPtr<UCanvasPanelSlot> ActiveDragSlot;
	TWeakObjectPtr<UCanvasPanel> ActiveDragCanvas;
	FVector2D LastMousePosition = FVector2D::ZeroVector;
	bool bDragging = false;
};
