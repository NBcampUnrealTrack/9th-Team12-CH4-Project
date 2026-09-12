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
#include "UI/Layer/TDHUDLayer.h"
#include "UI/Layer/WindowLayer/Common/WindowBase/TDWindowBaseWidget.h"

UTDBossHPWidget* UTDUIRootWidget::GetBossWidget() const
{
	if (!HUDLayer) return nullptr;
	for (UWidget* Child : HUDLayer->GetAllChildren())
	{
		if (UTDHUDLayer* HUD = Cast<UTDHUDLayer>(Child))
		{
			if (UTDBossHPWidget* BossWidget = HUD->GetBossWidget()) return BossWidget;
		}
	}
	return nullptr;
}

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
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(HUDReadyTimer);
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UTDUIManagerSubsystem* UIManager =
			LocalPlayer->GetSubsystem<UTDUIManagerSubsystem>())
		{
			UIManager->UnregisterRoot(this);
		}
	}

    DisplayedPawn.Reset();
    bPlayerHUDReady = false;
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
        return;
    }
    if (bPlayerHUDReady && DisplayedPawn.Get() == CharacterPawn)
    {
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
}
