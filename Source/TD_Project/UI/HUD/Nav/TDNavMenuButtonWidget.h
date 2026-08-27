#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HUD/Nav/TDNavMenuTypes.h"
#include "TDNavMenuButtonWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
class UTDHudNavButtonDA;
class UTDNavMenuButtonWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FTDOnNavMenuButtonClicked,
	UTDNavMenuButtonWidget*, ButtonWidget,
	ETDNavMenuType, MenuType
);

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDNavMenuButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Navigation")
	FTDOnNavMenuButtonClicked OnClicked;

	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void SetStyleData(UTDHudNavButtonDA* InStyleData);

	void SetMenuType(ETDNavMenuType InMenuType);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> Button;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Image;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ExposeOnSpawn = "true"))
	FText Label;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTDHudNavButtonDA> StyleData;

	ETDNavMenuType MenuType = ETDNavMenuType::Inventory;
	bool bSelected = false;

	void RefreshContent();
	void RefreshStyle();

	UFUNCTION()
	void HandleClicked();
};
