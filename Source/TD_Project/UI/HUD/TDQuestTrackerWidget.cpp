// Fill out your copyright notice in the Description page of Project Settings.
#include "UI/HUD/TDQuestTrackerWidget.h"

#include "Engine/World.h"
#include "Player/TDPlayerState.h"
#include "Quest/TDQuestComponent.h"
#include "TimerManager.h"

void UTDQuestTrackerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TryBindQuestComponent();

	if (BoundQuestComponent == nullptr)
	{
		GetWorld()->GetTimerManager().SetTimer(
			BindRetryTimerHandle,
			this,
			&UTDQuestTrackerWidget::
				TryBindQuestComponent,
			0.25f,
			true);
	}
}

void UTDQuestTrackerWidget::NativeDestruct()
{
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(
			BindRetryTimerHandle);
	}

	if (BoundQuestComponent != nullptr)
	{
		BoundQuestComponent
			->OnQuestListChanged
			.RemoveDynamic(
				this,
				&UTDQuestTrackerWidget::HandleQuestListChanged);
	}

	BoundQuestComponent = nullptr;

	Super::NativeDestruct();
}

void UTDQuestTrackerWidget::TryBindQuestComponent()
{
	if (BoundQuestComponent != nullptr)
	{
		return;
	}

	const ATDPlayerState* PlayerState =
		GetOwningPlayerState<ATDPlayerState>();

	if (PlayerState == nullptr)
	{
		return;
	}

	BoundQuestComponent =
		PlayerState->GetQuestComponent();

	if (BoundQuestComponent == nullptr)
	{
		return;
	}

	BoundQuestComponent
		->OnQuestListChanged
		.AddUniqueDynamic(
			this,
			&UTDQuestTrackerWidget::HandleQuestListChanged);

	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(
			BindRetryTimerHandle);
	}

	RefreshQuestTracker();
}

void UTDQuestTrackerWidget::HandleQuestListChanged()
{
	RefreshQuestTracker();
}

void UTDQuestTrackerWidget::RefreshQuestTracker()
{
	if (BoundQuestComponent == nullptr)
	{
		BP_OnRefreshQuestTracker(
			TArray<FTDQuestViewData>());
		return;
	}

	BP_OnRefreshQuestTracker(
		BoundQuestComponent
			->GetQuestTrackerViews());
}