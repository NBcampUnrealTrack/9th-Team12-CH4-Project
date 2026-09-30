#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Option/TDOptionView.h"
#include "TDOptionItemEntryWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTDOnOptionEntrySelected,
	int32,
	SlotIndex);

/** 추가 옵션 창의 목록 한 줄. 강화의 WBP_EnhanceItemEntry 와 같은 모양이다. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDOptionItemEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupEntry(const FTDOptionItemView& InItem, bool bSelected);

	/**
	 * 등급 태그를 화면 문구와 색으로. 목록과 상세가 같은 표기를 써야 해서 여기 모았다.
	 * 아이템 툴팁의 희귀도 표기(일반·희귀·영웅·전설)와 같은 말과 같은 색을 쓴다.
	 */
	static FText GetRarityText(FGameplayTag Rarity, FLinearColor& OutColor);

	FTDOnOptionEntrySelected OnSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BTN_Select;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IMG_ItemIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_ItemName;

	/** 강화 단계 자리에 옵션 등급을 보여준다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_OptionRarity;

private:
	UFUNCTION()
	void HandleClicked();

	void RefreshEntry();

	UPROPERTY(Transient)
	FTDOptionItemView ItemView;

	bool bEntrySelected = false;
};
