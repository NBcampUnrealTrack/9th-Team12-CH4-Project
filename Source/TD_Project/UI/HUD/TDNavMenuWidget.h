#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDNavMenuWidget.generated.h"

class UButton;
class UTDHudNavButtonDA;

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDNavMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// WBP 안의 버튼 이름과 정확히 같아야 합니다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UButton> InventoryButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UButton> QuestButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UButton> SystemButton;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Button Style")
		TObjectPtr<UTDHudNavButtonDA> ButtonStyleData;

private:
	UPROPERTY(Transient)
		TObjectPtr<UButton> SelectedButton;

	void RefreshButtonStyles();
	void SelectButton(UButton* NewSelectedButton);

	UFUNCTION()
		void HandleInventoryClicked();

	UFUNCTION()
		void HandleQuestClicked();

	UFUNCTION()
		void HandleSystemClicked();
};