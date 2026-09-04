#include "UI/HUD/TDPlayerStatusWidget.h"
#include "UI/ViewModel/TDPlayerStatsViewModel.h"
#include "UI/ViewModel/TDPlayerStatsSubsystem.h"
#include "UI/Common/TDProgressBarStyleDA.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	TArray<UE::FieldNotification::FFieldId> HUDFields()
	{
		using F = UTDPlayerStatsViewModel::FFieldNotificationClassDescriptor;
		return {
			F::LevelNameText, F::HealthPercent, F::ManaPercent, F::HealthText, F::ManaText,
			F::Health, F::MaxHealth, F::Mana, F::MaxMana, F::HasPlayerState
		};
	}

	const FTDProgressBarStyle& ResolveHealthStyle(const UTDProgressBarStyleDA* StyleData)
	{
		if (StyleData) return StyleData->Style;
		static const FTDProgressBarStyle Style = []
		{
			FTDProgressBarStyle Result;
			Result.Mode = ETDProgressBarAnimationMode::DamageTrail;
			return Result;
		}();
		return Style;
	}

	const FTDProgressBarStyle& ResolveManaStyle(const UTDProgressBarStyleDA* StyleData)
	{
		if (StyleData) return StyleData->Style;
		static const FTDProgressBarStyle Style;
		return Style;
	}
}

void UTDPlayerStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ViewModel){
		SetViewModel(ViewModel);
	}
	else if (ULocalPlayer* LP = GetOwningLocalPlayer()){
		UTDPlayerStatsSubsystem* Stats = LP->GetSubsystem<UTDPlayerStatsSubsystem>();
		Stats->RefreshSource();
		SetViewModel(Stats->GetPlayerStatsViewModel());
	}
}

void UTDPlayerStatusWidget::NativeDestruct()
{
	UnbindViewModel();
	bVitalsPending = false;
	HealthAnimation.Reset();
	ManaAnimation.Reset();
	Super::NativeDestruct();
}

void UTDPlayerStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const bool bWasPending = bVitalsPending;
	if (bVitalsPending){
		// 한 번의 GAS 갱신에서 현재값/최대값 알림이 연달아 온다. 최종 묶음만 받아 연출한다.
		bVitalsPending = false;
		RefreshVitalTargets(bAnimateVitals);
	}
	if (!bWasPending && !HealthAnimation.IsActive() && !ManaAnimation.IsActive()) return;
	HealthAnimation.Advance(InDeltaTime);
	ManaAnimation.Advance(InDeltaTime);
	ApplyVitalVisuals();
}

void UTDPlayerStatusWidget::UnbindViewModel()
{
	if (ViewModel) ViewModel->RemoveAllFieldValueChangedDelegates(this);
}

void UTDPlayerStatusWidget::SetViewModel(UTDPlayerStatsViewModel* InViewModel)
{
	UnbindViewModel();
	ViewModel = InViewModel;
	HealthAnimation.Reset();
	ManaAnimation.Reset();
	if (ViewModel){
		for (const auto Field : HUDFields()){
			ViewModel->AddFieldValueChangedDelegate(Field,
			                                        INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
					                                        this, &ThisClass::OnFieldChanged));
			RefreshField(Field);
		}
	}
	else{
		if (LevelNameText) LevelNameText->SetText(FText::GetEmpty());
		if (HealthValueText) HealthValueText->SetText(FText::GetEmpty());
		if (ManaValueText) ManaValueText->SetText(FText::GetEmpty());
		if (HealthBar) HealthBar->SetPercent(0.f);
		if (ManaBar) ManaBar->SetPercent(0.f);
	}
	// 처음 연결하거나 위젯을 다시 열 때는 남은 수치에서 바로 시작한다.
	RefreshVitalTargets(false);
	bVitalsPending = false;
	ApplyVitalVisuals();
}

void UTDPlayerStatusWidget::OnFieldChanged(UObject* Object, UE::FieldNotification::FFieldId Field)
{
	RefreshField(Field);
}

void UTDPlayerStatusWidget::RefreshField(UE::FieldNotification::FFieldId Field)
{
	if (!ViewModel) return;
	using F = UTDPlayerStatsViewModel::FFieldNotificationClassDescriptor;
	if (Field == F::LevelNameText && LevelNameText) LevelNameText->
			SetText(ViewModel->LevelNameText);
	else if (Field == F::HealthPercent || Field == F::ManaPercent || Field == F::Health
		|| Field == F::MaxHealth || Field == F::Mana || Field == F::MaxMana)
		bVitalsPending = true;
	else if (Field == F::HealthText && HealthValueText) HealthValueText->SetText(
			ViewModel->HealthText);
	else if (Field == F::ManaText && ManaValueText) ManaValueText->SetText(ViewModel->ManaText);
	else if (Field == F::HasPlayerState){
		HealthAnimation.Reset();
		ManaAnimation.Reset();
		bVitalsPending = true;
	}
}

void UTDPlayerStatusWidget::RefreshVitalTargets(bool bAnimate)
{
	if (!ViewModel || !ViewModel->HasPlayerState){
		HealthAnimation.Reset();
		ManaAnimation.Reset();
		return;
	}
	HealthAnimation.SetValue(ViewModel->Health, ViewModel->MaxHealth, ViewModel->HealthPercent,
	                         bAnimate, ResolveHealthStyle(HealthBarStyleData));
	ManaAnimation.SetValue(ViewModel->Mana, ViewModel->MaxMana, ViewModel->ManaPercent,
	                       bAnimate, ResolveManaStyle(ManaBarStyleData));
}

void UTDPlayerStatusWidget::ApplyVitalVisuals()
{
	if (HealthBar) HealthBar->SetPercent(HealthAnimation.DisplayPercent);
	if (ManaBar) ManaBar->SetPercent(ManaAnimation.DisplayPercent);
	if (HealthLagBar) HealthLagBar->SetPercent(HealthAnimation.TrailPercent);
	const FTDProgressBarStyle& HealthStyle = ResolveHealthStyle(HealthBarStyleData);
	const FTDProgressBarStyle& ManaStyle = ResolveManaStyle(ManaBarStyleData);
	if (HealthBarStyleData){
		if (HealthBar) HealthBar->SetFillColorAndOpacity(HealthStyle.FillTint);
		if (HealthLagBar) HealthLagBar->SetFillColorAndOpacity(HealthStyle.TrailTint);
		if (HealthRecoveryFlashBar) HealthRecoveryFlashBar->SetFillColorAndOpacity(
				HealthStyle.FlashTint);
	}
	if (ManaBarStyleData){
		if (ManaBar) ManaBar->SetFillColorAndOpacity(ManaStyle.FillTint);
		if (ManaRecoveryFlashBar) ManaRecoveryFlashBar->SetFillColorAndOpacity(ManaStyle.FlashTint);
	}
	if (HealthRecoveryFlashBar){
		HealthRecoveryFlashBar->SetPercent(HealthAnimation.DisplayPercent);
		HealthRecoveryFlashBar->SetRenderOpacity(
				HealthAnimation.FlashAlpha * FMath::Clamp(HealthStyle.FlashOpacity, 0.f, 1.f));
	}
	if (ManaRecoveryFlashBar){
		ManaRecoveryFlashBar->SetPercent(ManaAnimation.DisplayPercent);
		ManaRecoveryFlashBar->SetRenderOpacity(
				ManaAnimation.FlashAlpha * FMath::Clamp(ManaStyle.FlashOpacity, 0.f, 1.f));
	}
}

void UTDPlayerStatusWidget::SetMainPortrait(UTexture2D* Texture, FVector2D UVScale,
                                            FVector2D UVOffset)
{
	if (!MainPortrait || !Texture) return;
	if (UMaterialInstanceDynamic* Material = MainPortrait->GetDynamicMaterial()){
		Material->SetTextureParameterValue(TEXT("Portrait"), Texture);
		Material->SetVectorParameterValue(
				TEXT("PortraitUV"), FLinearColor(UVScale.X, UVScale.Y, UVOffset.X, UVOffset.Y));
	}
}
