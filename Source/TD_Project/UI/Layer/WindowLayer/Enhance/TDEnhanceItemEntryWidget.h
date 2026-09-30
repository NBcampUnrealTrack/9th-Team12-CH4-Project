#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enhance/TDEnhanceView.h"
#include "TDEnhanceItemEntryWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTDOnEnhanceEntrySelected,
	int32,
	SlotIndex);

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDEnhanceItemEntryWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupEntry(
		const FTDEnhanceItemView& InItem,
		bool bSelected);

	FTDOnEnhanceEntrySelected OnSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_Select;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IMG_ItemIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_ItemName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_EnhanceLevel;

private:
	UFUNCTION()
	void HandleClicked();

	void RefreshEntry();

	UPROPERTY(Transient)
	FTDEnhanceItemView ItemView;

	bool bEntrySelected = false;
};