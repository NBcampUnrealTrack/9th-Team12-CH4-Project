#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "TDItemSlotVisualWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;
class UWidget;


USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDItemSlotVisualData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|Item Slot")
	bool bHasItem = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|Item Slot")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|Item Slot")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|Item Slot", meta = (ClampMin = "0", UIMin = "0"))
	int32 Count = 0;

	/** 퀵슬롯처럼 0개와 1개도 표시해야 할 때 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|Item Slot")
	bool bAlwaysShowCount = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|Item Slot")
	FGameplayTag Rarity;
};

/**
 * 아이템 슬롯의 공용 표시 전용 위젯.
 */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDItemSlotVisualWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TD|Item Slot")
	void SetSlotVisualData(const FTDItemSlotVisualData& InData);

	UFUNCTION(BlueprintCallable, Category = "TD|Item Slot")
	void ClearSlotVisual();

	UFUNCTION(BlueprintCallable, Category = "TD|Item Slot")
	void SetSlotSelected(bool bInSelected);

	UFUNCTION(BlueprintCallable, Category = "TD|Item Slot")
	void SetSlotEnabled(bool bInEnabled);

	UFUNCTION(BlueprintPure, Category = "TD|Item Slot")
	FTDItemSlotVisualData GetSlotVisualData() const { return SlotVisualData; }

	UFUNCTION(BlueprintPure, Category = "TD|Item Slot")
	bool HasItem() const { return SlotVisualData.bHasItem; }

	UFUNCTION(BlueprintPure, Category = "TD|Item Slot")
	bool IsSlotSelected() const { return bSlotSelected; }

	UFUNCTION(BlueprintPure, Category = "TD|Item Slot")
	bool IsSlotHovered() const { return bSlotHovered; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	// 선택 시  테두리
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> GlowOuter;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> GlowMiddle;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectionFrame;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Item Slot", meta = (ExposeOnSpawn = "true"))
	FTDItemSlotVisualData SlotVisualData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Item Slot")
	bool bSlotSelected = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Item Slot")
	bool bSlotEnabled = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TD|Item Slot")
	bool bSlotHovered = false;

	/** WBP에서 배경, 희귀도 테두리, 선택 및 비활성 효과를 갱신한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TD|Item Slot", meta = (DisplayName = "On Slot Visual State Changed"))
	void BP_OnSlotVisualStateChanged(const FTDItemSlotVisualData& InData, bool bIsSelected, bool bInSlotEnabled);

private:
	void RefreshVisual();
	void RefreshInteractionVisual();
};
