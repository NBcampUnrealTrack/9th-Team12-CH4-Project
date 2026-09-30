// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HUD/Nav/TDNavMenuTypes.h"
#include "TDHUDLayer.generated.h"

class UTDNavMenuWidget;
class UTDQuestTrackerWidget;
class UTDChatWidget;
class UTDQuickSlotWidget;
class UTDEXPBarWidget;
class UTDPartyStatusWidget;
class UTDPlayerStatusWidget;
class UTDBossHPWidget;
/**
 * 
 */
UCLASS()
class TD_PROJECT_API UTDHUDLayer : public UUserWidget
{
	GENERATED_BODY()

public:
	UTDBossHPWidget* GetBossWidget() const { return WBP_BossHpBar; }

protected:
	/** 기존 WBP_HUD_Layer 내부의 위젯 이름에 맞춰 연결한다. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTDBossHPWidget> WBP_BossHpBar;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

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

private:
	UFUNCTION()
	void HandleMenuRequested(ETDNavMenuType MenuType);

	UFUNCTION()
	void HandleMenuWindowStateChanged(ETDNavMenuType MenuType, bool bIsOpen);
};
