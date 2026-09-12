// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "TDQuickSlotWidget.generated.h"

class UTDProgressionComponent;
class UTDInventoryComponent;
class UTDItemSlotVisualWidget;
class UTDQuickSlotComponent;

/**
 * 
 */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDQuickSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Index는 화면의 1~6번에 대응하는 0~5다. 반환값은 로컬 요청 가능 여부다. */
	UFUNCTION(BlueprintCallable, Category = "TD|QuickSlot")
	bool RequestRegisterItem(int32 Index, FName ItemId);
	UFUNCTION(BlueprintCallable, Category = "TD|QuickSlot")
	void RequestUseSlot(int32 Index);
	UFUNCTION(BlueprintCallable, Category = "TD|QuickSlot")
	void RequestClearSlot(int32 Index);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& Event) override;
	virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation) override;

private:
	void RefreshSources();
	void UnbindSources();
	void RefreshSlots();
    void RefreshSkillSlots();
    void RefreshSkillCooldowns();
    FTimerHandle SkillCooldownTimer;
    UPROPERTY(Transient) TArray<TObjectPtr<UTDItemSlotVisualWidget>> SkillWidgets;
    TWeakObjectPtr<UTDProgressionComponent> SkillSource;
    FName DisplayedSkillClass;
    TArray<int32> DisplayedSkillLevels;
    int32 DisplayedCharacterLevel = INDEX_NONE;
	int32 FindSlotAt(const FVector2D& ScreenPosition) const;
	UFUNCTION()
	void QueueRefresh();
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTDItemSlotVisualWidget>> SlotWidgets;
	UPROPERTY(Transient)
	TObjectPtr<UTDQuickSlotComponent> QuickSlots;
	UPROPERTY(Transient)
	TObjectPtr<UTDInventoryComponent> Inventory;
	FTimerHandle SourceCheckTimer;
	FTimerHandle RefreshTimer;
	int32 PressedSlot = INDEX_NONE;
	FKey PressedButton;
};
