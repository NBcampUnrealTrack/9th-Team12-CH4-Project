// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/TDQuestTypes.h"
#include "TDQuestTrackerWidget.generated.h"

class UTDQuestComponent;

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDQuestTrackerWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable,
		Category = "TD|Quest")
	void RefreshQuestTracker();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * 블루프린트는 전달받은 배열을 화면에 그리기만 한다.
	 * 메인 1개 + 서브/일일 2개 제한과 정렬은 이미 C++에서 끝난 상태다.
	 */
	UFUNCTION(BlueprintImplementableEvent,
		Category = "TD|Quest",
		DisplayName = "On Refresh Quest Tracker")
	void BP_OnRefreshQuestTracker(
		const TArray<FTDQuestViewData>& QuestViews);

private:
	void TryBindQuestComponent();

	UFUNCTION()
	void HandleQuestListChanged();

	UPROPERTY(Transient)
	TObjectPtr<UTDQuestComponent> BoundQuestComponent;

	FTimerHandle BindRetryTimerHandle;
};