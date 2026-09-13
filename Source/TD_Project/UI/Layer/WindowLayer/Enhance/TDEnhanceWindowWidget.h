#pragma once

#include "CoreMinimal.h"
#include "Enhance/TDEnhanceView.h"
#include "UI/Layer/WindowLayer/Common/WindowBase/TDWindowBaseWidget.h"
#include "TDEnhanceWindowWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UVerticalBox;
class UTDEnhanceServiceComponent;
class UTDEnhanceItemEntryWidget;

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDEnhanceWindowWidget
	: public UTDWindowBaseWidget
{
	GENERATED_BODY()

public:
	void InitializeService(UTDEnhanceServiceComponent* InService);
	void ApplyView(const FTDEnhanceWindowView& InView);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditDefaultsOnly, Category = "TD|Enhance")
	TSubclassOf<UTDEnhanceItemEntryWidget> ItemEntryClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> VB_Items;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Gold;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Empty;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IMG_SelectedItem;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_ItemName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Level;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Cost;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_SuccessRate;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_FailureRule;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_Enhance;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Result;

private:
	void RefreshWindow();
	void RefreshList();
	void RefreshDetail();

	const FTDEnhanceItemView* FindSelectedItem() const;

	UFUNCTION()
	void HandleItemSelected(int32 SlotIndex);

	UFUNCTION()
	void HandleEnhanceClicked();

	TWeakObjectPtr<UTDEnhanceServiceComponent> Service;

	UPROPERTY(Transient)
	FTDEnhanceWindowView CurrentView;

	int32 SelectedSlot = INDEX_NONE;
	int32 RequestCounter = 0;
	int32 PendingRequestId = 0;
};