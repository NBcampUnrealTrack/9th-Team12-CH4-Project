#include "UI/HUD/TDEXPBarWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "UI/ViewModel/TDPlayerStatsSubsystem.h"
#include "UI/ViewModel/TDPlayerStatsViewModel.h"
#include "UI/Common/TDProgressBarStyleDA.h"

namespace
{
	const FTDProgressBarStyle& ResolveExpStyle(const UTDProgressBarStyleDA* StyleData)
	{
		if (StyleData) return StyleData->Style;
		static const FTDProgressBarStyle Style = []
		{
			FTDProgressBarStyle Result;
			Result.Mode = ETDProgressBarAnimationMode::LevelWrapped;
			Result.ChangeSeconds = 0.35f;
			Result.FlashSeconds = 0.4f;
			Result.FlashOpacity = 0.4f;
			return Result;
		}();
		return Style;
	}
}

void UTDEXPBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ViewModel) SetViewModel(ViewModel);
	else if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer()){
		if (UTDPlayerStatsSubsystem* Stats = LocalPlayer->GetSubsystem<UTDPlayerStatsSubsystem>()){
			Stats->RefreshSource();
			SetViewModel(Stats->GetPlayerStatsViewModel());
		}
	}
}

void UTDEXPBarWidget::NativeDestruct()
{
	UnbindViewModel();
	Animation.Reset();
	bProgressPending = false;
	Super::NativeDestruct();
}

void UTDEXPBarWidget::SetViewModel(UTDPlayerStatsViewModel* InViewModel)
{
	UnbindViewModel();
	ViewModel = InViewModel;
	Animation.Reset();
	bProgressPending = false;
	if (ViewModel){
		using F = UTDPlayerStatsViewModel::FFieldNotificationClassDescriptor;
		for (const auto Field : {
			     F::Level, F::TotalExp, F::ExpToNextLevel, F::ExpPercent, F::HasPlayerState
		     })
			ViewModel->AddFieldValueChangedDelegate(Field,
			                                        INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
					                                        this, &ThisClass::OnFieldChanged));
	}
	RefreshProgress(false);
	ApplyVisuals();
}

void UTDEXPBarWidget::UnbindViewModel()
{
	if (ViewModel) ViewModel->RemoveAllFieldValueChangedDelegates(this);
}

void UTDEXPBarWidget::OnFieldChanged(UObject* Object, UE::FieldNotification::FFieldId Field)
{
	if (Field == UTDPlayerStatsViewModel::FFieldNotificationClassDescriptor::HasPlayerState)
		Animation.Reset();
	bProgressPending = true;
}

void UTDEXPBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const bool bWasPending = bProgressPending;
	if (bProgressPending){
		// 같은 갱신의 레벨/누적 EXP/진행률을 한 묶음으로 반영한다. 숫자는 보간하지 않는다.
		bProgressPending = false;
		RefreshProgress(bAnimateExperience);
	}
	if (!bWasPending && !Animation.IsActive()) return;
	Animation.Advance(InDeltaTime);
	ApplyVisuals();
}

void UTDEXPBarWidget::RefreshProgress(bool bAnimate)
{
	if (!ViewModel || !ViewModel->HasPlayerState){
		Animation.Reset();
		if (ExpPercentText) ExpPercentText->SetText(NSLOCTEXT("TDEXPBar", "Waiting", "EXP —"));
		SetToolTipText(FText::GetEmpty());
		return;
	}
	Animation.SetWrappedTarget(ViewModel->Level, ViewModel->TotalExp, ViewModel->ExpPercent,
	                           bAnimate, ResolveExpStyle(ExpBarStyleData));

	const bool bMaxLevel = ViewModel->ExpToNextLevel == 0 && ViewModel->ExpPercent >= 1.f;
	FNumberFormattingOptions Format;
	Format.SetMinimumFractionalDigits(1);
	Format.SetMaximumFractionalDigits(1);
	if (ExpPercentText)
		ExpPercentText->SetText(bMaxLevel
			                        ? NSLOCTEXT("TDEXPBar", "Max", "EXP MAX")
			                        : FText::Format(
					                        NSLOCTEXT("TDEXPBar", "Percent", "EXP {0}"),
					                        FText::AsPercent(ViewModel->ExpPercent, &Format)));
	SetToolTipText(bMaxLevel
		               ? NSLOCTEXT("TDEXPBar", "MaxTooltip", "최고 레벨입니다.")
		               : FText::Format(NSLOCTEXT("TDEXPBar", "Tooltip", "Lv.{0} · 다음 레벨까지 {1} EXP"),
		                               FText::AsNumber(ViewModel->Level),
		                               FText::AsNumber(ViewModel->ExpToNextLevel)));
}

void UTDEXPBarWidget::ApplyVisuals()
{
	if (ExpBar) ExpBar->SetPercent(Animation.DisplayPercent);
	const FTDProgressBarStyle& Style = ResolveExpStyle(ExpBarStyleData);
	if (ExpBarStyleData){
		if (ExpBar) ExpBar->SetFillColorAndOpacity(Style.FillTint);
		if (ExpFlashBar) ExpFlashBar->SetFillColorAndOpacity(Style.FlashTint);
	}
	if (ExpFlashBar){
		ExpFlashBar->SetPercent(Animation.DisplayPercent);
		ExpFlashBar->SetRenderOpacity(
				Animation.FlashAlpha * FMath::Clamp(Style.FlashOpacity, 0.f, 1.f));
	}
}
