// Fill out your copyright notice in the Description page of Project Settings.


#include "TDUIRootWidget.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Player/TDPlayerState.h"
#include "Player/TDPlayerController.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "UI/HUD/TDPlayerStatusWidget.h"
#include "UI/ViewModel/TDPlayerStatsSubsystem.h"
#include "UI/ViewModel/TDPlayerStatsViewModel.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Engine/LocalPlayer.h"
#include "UI/Core/TDUIManagerSubsystem.h"
#include "UI/Layer/WindowLayer/Common/WindowBase/TDWindowBaseWidget.h"
#include "Character/TDPlayerCharacter.h"
#include "UI/HUD/TDRespawnWidget.h"
#include "UI/Settings/TDUISettings.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

FReply UTDUIRootWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// 입력창에 포커스가 있어도 Alt 처리 
	if (IsPlayerHUDReady()
		&& (InKeyEvent.GetKey() == EKeys::LeftAlt))
	{
		if (ATDPlayerController* Controller = Cast<ATDPlayerController>(GetOwningPlayer()))
		{
			if (!InKeyEvent.IsRepeat())
			{
				Controller->ToggleMouseCursor();
			}
			return FReply::Handled();
		}
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UTDUIRootWidget::NativeConstruct()
{
    Super::NativeConstruct();
    bPlayerHUDReady = false;
    DisplayedPawn.Reset();
    if (HUDLayer) HUDLayer->SetVisibility(ESlateVisibility::Collapsed);
    RefreshPlayerHUD();
    if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(HUDReadyTimer, this, &ThisClass::RefreshPlayerHUD, 0.1f, true);

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UTDUIManagerSubsystem* UIManager =
			LocalPlayer->GetSubsystem<UTDUIManagerSubsystem>())
		{
			UIManager->RegisterRoot(this);
		}
	}
}

void UTDUIRootWidget::NativeDestruct()
{
    UnbindDeathSource();
    CloseDeathUI();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(HUDReadyTimer);
    DisplayedPawn.Reset();
    bPlayerHUDReady = false;
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UTDUIManagerSubsystem* UIManager =
			LocalPlayer->GetSubsystem<UTDUIManagerSubsystem>())
		{
			UIManager->UnregisterRoot(this);
		}
	}

	Super::NativeDestruct();
}

void UTDUIRootWidget::SetLoadingVisible(bool bVisible)
{
	if (!IsValid(LoadingLayer))
	{
		return;
	}

	LoadingLayer->SetVisibility(
		bVisible
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed
	);
}

bool UTDUIRootWidget::AddWindow(UTDWindowBaseWidget* WindowWidget)
{
	if (!IsValid(WindowLayer) || !IsValid(WindowWidget))
	{
		return false;
	}

	UCanvasPanelSlot* WindowSlot = WindowLayer->AddChildToCanvas(WindowWidget);
	if (!WindowSlot)
	{
		return false;
	}

	WindowSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	WindowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	WindowSlot->SetPosition(FVector2D::ZeroVector);
	WindowSlot->SetAutoSize(true);
	return true;
}

namespace
{
    // 숨겨져 있던 체력창의 연출값도 현재 데이터로 맞춘 후 표시한다.
    void RefreshStatusWidgets(UWidget* Widget, UTDPlayerStatsViewModel* ViewModel)
    {
        if (UTDPlayerStatusWidget* Status = Cast<UTDPlayerStatusWidget>(Widget)) Status->SetViewModel(ViewModel);
        else if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
        {
            if (UserWidget->WidgetTree) RefreshStatusWidgets(UserWidget->WidgetTree->RootWidget, ViewModel);
        }
        else if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
        {
            for (UWidget* Child : Panel->GetAllChildren()) RefreshStatusWidgets(Child, ViewModel);
        }
    }
}

void UTDUIRootWidget::RefreshPlayerHUD()
{
    if (!HUDLayer) return;
    APlayerController* Controller = GetOwningPlayer();
    ATDPlayerState* State = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;
    APawn* CharacterPawn = Controller ? Controller->GetPawn() : nullptr;
    const UAbilitySystemComponent* ASC = State ? State->GetAbilitySystemComponent() : nullptr;
    const bool bReady = !bWaitForCharacterLoad || (State && State->HasSelectedCharacter()
        && CharacterPawn && CharacterPawn->GetPlayerState() == State
        && ASC && ASC->GetAvatarActor() == CharacterPawn);
    if (!bReady)
    {
        HUDLayer->SetVisibility(ESlateVisibility::Collapsed);
        bPlayerHUDReady = false;
        DisplayedPawn.Reset();
        RefreshDeathUI();
        return;
    }
    if (bPlayerHUDReady && DisplayedPawn.Get() == CharacterPawn)
    {
        RefreshDeathUI();
        return;
    }
    if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
    {
        if (UTDPlayerStatsSubsystem* Stats = LocalPlayer->GetSubsystem<UTDPlayerStatsSubsystem>())
        {
            Stats->RefreshSource();
            UTDPlayerStatsViewModel* ViewModel = Stats->GetPlayerStatsViewModel();
            if (ViewModel)
            {
                ViewModel->RefreshAll();
                RefreshStatusWidgets(HUDLayer, ViewModel);
            }
        }
    }
    DisplayedPawn = CharacterPawn;
    bPlayerHUDReady = true;
    HUDLayer->SetVisibility(LoadedHUDVisibility);
    RefreshDeathUI();
}

void UTDUIRootWidget::UnbindDeathSource()
{
    if (DeathSource.IsValid())
    {
        DeathSource->OnDeath.RemoveDynamic(this, &ThisClass::RefreshDeathUI);
        DeathSource->OnRespawn.RemoveDynamic(this, &ThisClass::RefreshDeathUI);
    }
    DeathSource.Reset();
}

void UTDUIRootWidget::RefreshDeathUI()
{
    APlayerController* Controller = GetOwningPlayer();
    ATDPlayerCharacter* Character = Controller && Controller->IsLocalController()
        ? Cast<ATDPlayerCharacter>(Controller->GetPawn()) : nullptr;
    if (DeathSource.Get() != Character)
    {
        UnbindDeathSource();
        CloseDeathUI();
        DeathSource = Character;
        if (Character)
        {
            Character->OnDeath.AddUniqueDynamic(this, &ThisClass::RefreshDeathUI);
            Character->OnRespawn.AddUniqueDynamic(this, &ThisClass::RefreshDeathUI);
        }
    }
    if (!Character || !Character->IsDead() || !IsPlayerHUDReady())
    {
        CloseDeathUI();
        return;
    }
    if (DeathScreen || !ModalStack || !IsPlayerHUDReady()) return;
    const TSubclassOf<UTDRespawnWidget> WidgetClass = GetDefault<UTDUISettings>()->RespawnWidgetClass.LoadSynchronous();
    if (!WidgetClass) return;
    bCursorVisibleBeforeDeath = Controller->bShowMouseCursor;
    DeathScreen = ModalStack->AddWidget<UTDRespawnWidget>(WidgetClass);
    if (DeathScreen)
    {
        FInputModeUIOnly Mode;
        UWidget* FocusTarget = DeathScreen->GetDesiredFocusTarget();
        Mode.SetWidgetToFocus(FocusTarget ? FocusTarget->TakeWidget() : DeathScreen->TakeWidget());
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Controller->SetInputMode(Mode);
        Controller->bShowMouseCursor = true;
    }
}

void UTDUIRootWidget::CloseDeathUI()
{
    if (!DeathScreen) return;
    if (ModalStack) ModalStack->RemoveWidget(*DeathScreen);
    DeathScreen = nullptr;
    if (APlayerController* Controller = GetOwningPlayer(); Controller && Controller->IsLocalController() && IsPlayerHUDReady())
    {
        Controller->bShowMouseCursor = bCursorVisibleBeforeDeath;
        if (bCursorVisibleBeforeDeath)
        {
            FInputModeGameAndUI Mode;
            Mode.SetHideCursorDuringCapture(false);
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Controller->SetInputMode(Mode);
        }
        else
        {
            FInputModeGameOnly Mode;
            Mode.SetConsumeCaptureMouseDown(false);
            Controller->SetInputMode(Mode);
        }
    }
}
