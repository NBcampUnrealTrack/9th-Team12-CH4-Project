#pragma once

#include "CoreMinimal.h"
#include "Option/TDOptionView.h"
#include "UI/Layer/WindowLayer/Common/WindowBase/TDWindowBaseWidget.h"
#include "TDOptionWindowWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UVerticalBox;
class UTDOptionServiceComponent;
class UTDOptionItemEntryWidget;

/** 추가 옵션(잠재능력) 재설정 창. 강화 창(UTDEnhanceWindowWidget)과 같은 구성이다. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDOptionWindowWidget : public UTDWindowBaseWidget
{
	GENERATED_BODY()

public:
	void InitializeService(UTDOptionServiceComponent* InService);
	void ApplyView(const FTDOptionWindowView& InView);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditDefaultsOnly, Category = "TD|Option")
	TSubclassOf<UTDOptionItemEntryWidget> ItemEntryClass;

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

	/** 현재 옵션 등급. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Rarity;

	/** 지금 붙어 있는 옵션 세 줄. 줄바꿈으로 이어 붙인다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_CurrentOptions;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Cost;

	/** 등급 상승 확률과 주의 문구. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_UpgradeRule;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_Reroll;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Result;

private:
	void RefreshWindow();
	void RefreshList();
	void RefreshDetail();

	const FTDOptionItemView* FindSelectedItem() const;

	UFUNCTION()
	void HandleItemSelected(int32 SlotIndex);

	UFUNCTION()
	void HandleRerollClicked();

	TWeakObjectPtr<UTDOptionServiceComponent> Service;

	UPROPERTY(Transient)
	FTDOptionWindowView CurrentView;

	int32 SelectedSlot = INDEX_NONE;
	int32 RequestCounter = 0;
	int32 PendingRequestId = 0;
};
