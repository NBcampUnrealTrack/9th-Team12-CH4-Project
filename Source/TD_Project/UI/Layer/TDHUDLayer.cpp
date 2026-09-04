// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Layer/TDHUDLayer.h"

#include "Engine/LocalPlayer.h"
#include "UI/Core/TDUIManagerSubsystem.h"
#include "UI/HUD/TDNavMenuWidget.h"

void UTDHUDLayer::NativeConstruct()
{
	Super::NativeConstruct();

	if (NavMenu)
	{
		NavMenu->OnMenuRequested.AddUniqueDynamic(
			this,
			&ThisClass::HandleMenuRequested);
	}

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UTDUIManagerSubsystem* UIManager =
			LocalPlayer->GetSubsystem<UTDUIManagerSubsystem>())
		{
			UIManager->OnMenuWindowStateChanged.AddUniqueDynamic(
				this,
				&ThisClass::HandleMenuWindowStateChanged);

			if (NavMenu)
			{
				NavMenu->SetMenuSelected(
					ETDNavMenuType::Inventory,
					UIManager->IsMenuOpen(ETDNavMenuType::Inventory));
			}
		}
	}
}

void UTDHUDLayer::NativeDestruct()
{
	if (NavMenu)
	{
		NavMenu->OnMenuRequested.RemoveDynamic(
			this,
			&ThisClass::HandleMenuRequested);
	}

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UTDUIManagerSubsystem* UIManager =
			LocalPlayer->GetSubsystem<UTDUIManagerSubsystem>())
		{
			UIManager->OnMenuWindowStateChanged.RemoveDynamic(
				this,
				&ThisClass::HandleMenuWindowStateChanged);
		}
	}

	Super::NativeDestruct();
}

void UTDHUDLayer::HandleMenuRequested(ETDNavMenuType MenuType)
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	if (UTDUIManagerSubsystem* UIManager =
		LocalPlayer->GetSubsystem<UTDUIManagerSubsystem>())
	{
		UIManager->RequestMenu(MenuType);
	}
}

void UTDHUDLayer::HandleMenuWindowStateChanged(
	ETDNavMenuType MenuType,
	bool bIsOpen)
{
	if (NavMenu)
	{
		NavMenu->SetMenuSelected(MenuType, bIsOpen);
	}
}
