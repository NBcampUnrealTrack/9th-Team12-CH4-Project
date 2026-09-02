#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDEnemyHealthBarTestWidget.generated.h"

class UButton;
class USpinBox;
class UTextBlock;
class UTDEnemyHealthBarWidget;

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDEnemyHealthBarTestWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDEnemyHealthBarWidget> EnemyHealthBarPreview;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USpinBox> CurrentHealthInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USpinBox> MaxHealthInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USpinBox> StepInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> ApplyButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> DamageButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> CriticalDamageButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> HealButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> ResetButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION()
	void HandleApplyClicked();

	UFUNCTION()
	void HandleDamageClicked();

	UFUNCTION()
	void HandleCriticalDamageClicked();

	UFUNCTION()
	void HandleHealClicked();

	UFUNCTION()
	void HandleResetClicked();

	void ApplyHealthToPreview(bool bIsCritical = false);
	float GetSafeMaxHealth() const;
	float GetSafeStep() const;
};
