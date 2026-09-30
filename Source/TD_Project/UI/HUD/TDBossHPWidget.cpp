#include "UI/HUD/TDBossHPWidget.h"

#include "Components/ProgressBar.h"
#include "UI/Common/Typography/TDTextBlock.h"
#include "UI/ViewModel/TDBossViewModel.h"

#define LOCTEXT_NAMESPACE "TDBossHPWidget"

void UTDBossHPWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	BossLevel = FMath::Max(0, BossLevel);
	NormalizeHealth();
	RefreshDisplay();
}

void UTDBossHPWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetViewModel(ViewModel.Get());
}

void UTDBossHPWidget::NativeDestruct()
{
	if (ViewModel){
		ViewModel->OnDisplayChanged.RemoveDynamic(
				this, &ThisClass::RefreshFromViewModel);
	}

	Super::NativeDestruct();
}

void UTDBossHPWidget::SetViewModel(UTDBossViewModel* InViewModel)
{
	if (ViewModel){
		ViewModel->OnDisplayChanged.RemoveDynamic(
				this, &ThisClass::RefreshFromViewModel);
	}

	ViewModel = InViewModel;

	if (ViewModel){
		ViewModel->OnDisplayChanged.AddUniqueDynamic(
				this, &ThisClass::RefreshFromViewModel);
	}

	// 다음 이벤트를 기다리지 않고 현재 값부터 표시한다.
	RefreshFromViewModel();
}

void UTDBossHPWidget::RefreshFromViewModel()
{
	if (!ViewModel || !ViewModel->bHasBoss){
		ClearBossDisplay();
		return;
	}

	SetBossDisplay(
			ViewModel->BossName,
			ViewModel->BossLevel,
			ViewModel->Health,
			ViewModel->MaxHealth);
}


void UTDBossHPWidget::SetBossDisplay(const FText& InBossName, int32 InBossLevel,
                                     float InHealth, float InMaxHealth)
{
	BossName = InBossName;
	BossLevel = FMath::Max(0, InBossLevel);
	SetHealth(InHealth, InMaxHealth);
}

void UTDBossHPWidget::SetHealth(float InHealth, float InMaxHealth)
{
	Health = InHealth;
	MaxHealth = InMaxHealth;
	NormalizeHealth();
	RefreshDisplay();
}

void UTDBossHPWidget::ClearBossDisplay()
{
	BossName = FText::GetEmpty();
	BossLevel = 0;
	Health = MaxHealth = 0.f;
	RefreshDisplay();
}

float UTDBossHPWidget::GetHealthPercent() const
{
	return MaxHealth > KINDA_SMALL_NUMBER ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f;
}


void UTDBossHPWidget::NormalizeHealth()
{
	MaxHealth = FMath::IsFinite(MaxHealth) ? FMath::Max(0.f, MaxHealth) : 0.f;
	Health = FMath::IsFinite(Health) ? FMath::Clamp(Health, 0.f, MaxHealth) : 0.f;
}

void UTDBossHPWidget::RefreshDisplay()
{
	if (BossNameText) BossNameText->SetText(BossName);
	if (BossLevelText){
		BossLevelText->SetText(BossLevel > 0
			                       ? FText::Format(
					                       LOCTEXT("BossLevel", "Lv. {0}"),
					                       FText::AsNumber(BossLevel))
			                       : FText::GetEmpty());
	}
	if (HealthBar) HealthBar->SetPercent(GetHealthPercent());
	if (HealthValueText){
		FNumberFormattingOptions Format;
		Format.SetMaximumFractionalDigits(0);
		Format.SetMinimumFractionalDigits(0);
		HealthValueText->SetText(MaxHealth > KINDA_SMALL_NUMBER
			                         ? FText::Format(LOCTEXT("BossHealth", "{0} / {1}"),
			                                         FText::AsNumber(Health, &Format),
			                                         FText::AsNumber(MaxHealth, &Format))
			                         : FText::GetEmpty());
	}
}

#undef LOCTEXT_NAMESPACE
