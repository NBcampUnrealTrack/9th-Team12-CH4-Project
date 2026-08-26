// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDHUDLayer.generated.h"

class UTDNavMenuWidget;
class UTDQuestTrackerWidget;
class UTDChatWidget;
class UTDQuickSlotWidget;
class UTDEXPBarWidget;
class UTDPartyStatusWidget;
class UTDPlayerStatusWidget;
/**
 * 
 */
UCLASS()
class TD_PROJECT_API UTDHUDLayer : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UTDPlayerStatusWidget> PlayerStatus;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UTDEXPBarWidget> EXPBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UTDPartyStatusWidget> PartyStatus;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UTDQuickSlotWidget> QuickSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UTDChatWidget> Chat;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UTDQuestTrackerWidget> QuestTracker;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
		TObjectPtr<UTDNavMenuWidget> NavMenu;
	
};
