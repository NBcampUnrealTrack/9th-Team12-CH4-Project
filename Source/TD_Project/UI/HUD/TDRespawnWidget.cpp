#include "UI/HUD/TDRespawnWidget.h"
#include "Character/TDPlayerCharacter.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Input/CommonUIInputTypes.h"
#include "Player/TDPlayerController.h"

void UTDRespawnWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RespawnButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestRespawn);
}

void UTDRespawnWidget::NativeDestruct()
{
    UnbindRespawnRequestResult();
    Super::NativeDestruct();
}

void UTDRespawnWidget::UnbindRespawnRequestResult()
{
    if (ATDPlayerController* Controller = BoundRespawnController.Get())
        Controller->OnRespawnRequestResult.RemoveDynamic(this, &ThisClass::HandleRespawnRequestResult);
    BoundRespawnController.Reset();
}

void UTDRespawnWidget::NativeOnDeactivated()
{
    UnbindRespawnRequestResult();
    Super::NativeOnDeactivated();
}

void UTDRespawnWidget::HandleRespawnRequestResult(bool bSucceeded)
{
    if (!bSucceeded && IsActivated())
    {
        bRequestSent = false;
        RespawnButton->SetIsEnabled(true);
        RespawnButton->SetUserFocus(GetOwningPlayer());
    }
    // Success still waits for OnRespawn / the replicated alive state to close the UI.
}

void UTDRespawnWidget::NativeOnActivated()
{
    UnbindRespawnRequestResult();
    if (ATDPlayerController* Controller = Cast<ATDPlayerController>(GetOwningPlayer()))
    {
        BoundRespawnController = Controller;
        Controller->OnRespawnRequestResult.AddUniqueDynamic(this, &ThisClass::HandleRespawnRequestResult);
    }
    bRequestSent = false;
    RespawnButton->SetIsEnabled(true);
    Super::NativeOnActivated();
    RefreshCountdown();
}

void UTDRespawnWidget::RequestRespawn()
{
    ATDPlayerController* Controller = Cast<ATDPlayerController>(GetOwningPlayer());
    const ATDPlayerCharacter* Character = Controller ? Cast<ATDPlayerCharacter>(Controller->GetPawn()) : nullptr;
    if (bRequestSent || !Controller || !Controller->IsLocalController() || !Character || !Character->IsDead()) return;
    bRequestSent = true;
    RespawnButton->SetIsEnabled(false);
    Controller->ServerRequestRespawn();
    // Keep the screen until the authoritative alive state arrives.
}

void UTDRespawnWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    RefreshCountdown();
}

void UTDRespawnWidget::RefreshCountdown()
{
    if (!CountdownText) return;
    const ATDPlayerCharacter* Character = Cast<ATDPlayerCharacter>(GetOwningPlayerPawn());
    const float Remaining = Character ? Character->GetAutoRespawnRemainingSeconds() : -1.f;
    CountdownText->SetVisibility(Remaining >= 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (Remaining >= 0.f)
    {
        CountdownText->SetText(FText::Format(CountdownFormat,
            FFormatNamedArguments{{TEXT("Seconds"), FText::AsNumber(FMath::CeilToInt(Remaining))}}));
    }
}

TOptional<FUIInputConfig> UTDRespawnWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

UWidget* UTDRespawnWidget::NativeGetDesiredFocusTarget() const
{
    return RespawnButton;
}
