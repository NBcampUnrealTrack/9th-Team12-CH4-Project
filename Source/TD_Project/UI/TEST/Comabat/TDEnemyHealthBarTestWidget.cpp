#include "TDEnemyHealthBarTestWidget.h"

#include "UI/Combat/TDEnemyHealthBarWidget.h"
#include "Components/Button.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "TDEnemyHealthBarTestWidget"

void UTDEnemyHealthBarTestWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ApplyButton)
	{
		ApplyButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleApplyClicked);
	}
	if (DamageButton)
	{
		DamageButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDamageClicked);
	}
	if (CriticalDamageButton)
	{
		CriticalDamageButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCriticalDamageClicked);
	}
	if (HealButton)
	{
		HealButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleHealClicked);
	}
	if (ResetButton)
	{
		ResetButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResetClicked);
	}

	if (MaxHealthInput && MaxHealthInput->GetValue() <= 0.0f)
	{
		MaxHealthInput->SetValue(100.0f);
	}
	if (StepInput && StepInput->GetValue() <= 0.0f)
	{
		StepInput->SetValue(10.0f);
	}
	if (CurrentHealthInput && CurrentHealthInput->GetValue() <= 0.0f)
	{
		CurrentHealthInput->SetValue(GetSafeMaxHealth());
	}

	ApplyHealthToPreview();
}

void UTDEnemyHealthBarTestWidget::NativeDestruct()
{
	if (ApplyButton)
	{
		ApplyButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleApplyClicked);
	}
	if (DamageButton)
	{
		DamageButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleDamageClicked);
	}
	if (CriticalDamageButton)
	{
		CriticalDamageButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCriticalDamageClicked);
	}
	if (HealButton)
	{
		HealButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleHealClicked);
	}
	if (ResetButton)
	{
		ResetButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleResetClicked);
	}

	Super::NativeDestruct();
}

void UTDEnemyHealthBarTestWidget::HandleApplyClicked()
{
	ApplyHealthToPreview();
}

void UTDEnemyHealthBarTestWidget::HandleDamageClicked()
{
	if (!CurrentHealthInput)
	{
		return;
	}

	const float NewHealth = FMath::Max(
		0.0f,
		CurrentHealthInput->GetValue() - GetSafeStep());

	CurrentHealthInput->SetValue(NewHealth);
	ApplyHealthToPreview();
}

void UTDEnemyHealthBarTestWidget::HandleCriticalDamageClicked()
{
	if (!CurrentHealthInput)
	{
		return;
	}

	const float NewHealth = FMath::Max(
		0.0f,
		CurrentHealthInput->GetValue() - GetSafeStep());

	CurrentHealthInput->SetValue(NewHealth);
	ApplyHealthToPreview(true);
}

void UTDEnemyHealthBarTestWidget::HandleHealClicked()
{
	if (!CurrentHealthInput)
	{
		return;
	}

	const float NewHealth = FMath::Min(
		GetSafeMaxHealth(),
		CurrentHealthInput->GetValue() + GetSafeStep());

	CurrentHealthInput->SetValue(NewHealth);
	ApplyHealthToPreview();
}

void UTDEnemyHealthBarTestWidget::HandleResetClicked()
{
	const float MaxHealth = GetSafeMaxHealth();

	if (CurrentHealthInput)
	{
		CurrentHealthInput->SetValue(MaxHealth);
	}

	ApplyHealthToPreview();
}

void UTDEnemyHealthBarTestWidget::ApplyHealthToPreview(bool bIsCritical)
{
	const float MaxHealth = GetSafeMaxHealth();
	const float CurrentHealth = CurrentHealthInput
		? FMath::Clamp(CurrentHealthInput->GetValue(), 0.0f, MaxHealth)
		: MaxHealth;

	if (CurrentHealthInput)
	{
		CurrentHealthInput->SetValue(CurrentHealth);
	}

	if (EnemyHealthBarPreview)
	{
		EnemyHealthBarPreview->SetHealth(CurrentHealth, MaxHealth, bIsCritical);
	}

	if (StatusText)
	{
		const int32 Percent = MaxHealth > KINDA_SMALL_NUMBER
			? FMath::RoundToInt(CurrentHealth / MaxHealth * 100.0f)
			: 0;

		StatusText->SetText(FText::Format(
			LOCTEXT("HealthStatusFormat", "HP {0} / {1}  ({2}%)"),
			FText::AsNumber(FMath::RoundToInt(CurrentHealth)),
			FText::AsNumber(FMath::RoundToInt(MaxHealth)),
			FText::AsNumber(Percent)));
	}
}

float UTDEnemyHealthBarTestWidget::GetSafeMaxHealth() const
{
	return MaxHealthInput
		? FMath::Max(1.0f, MaxHealthInput->GetValue())
		: 100.0f;
}

float UTDEnemyHealthBarTestWidget::GetSafeStep() const
{
	return StepInput
		? FMath::Max(1.0f, StepInput->GetValue())
		: 10.0f;
}

#undef LOCTEXT_NAMESPACE
