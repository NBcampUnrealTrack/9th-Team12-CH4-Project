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
	RefreshQuestTracker();

	if (GetWorld() != nullptr)
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
	const ATDPlayerState* PlayerState =
		GetOwningPlayerState<ATDPlayerState>();
	UTDQuestComponent* Current = IsValid(PlayerState) && PlayerState->HasSelectedCharacter()
		? PlayerState->GetQuestComponent() : nullptr;
	if (BoundQuestComponent == Current)
	{
		return;
	}
	if (IsValid(BoundQuestComponent))
		BoundQuestComponent->OnQuestListChanged.RemoveDynamic(this, &UTDQuestTrackerWidget::HandleQuestListChanged);
	BoundQuestComponent = Current;
	if (IsValid(BoundQuestComponent))
		BoundQuestComponent
		->OnQuestListChanged
		.AddUniqueDynamic(
			this,
			&UTDQuestTrackerWidget::HandleQuestListChanged);

	RefreshQuestTracker();
}

void UTDQuestTrackerWidget::HandleQuestListChanged()
{
	RefreshQuestTracker();
}

void UTDQuestTrackerWidget::RefreshQuestTracker()
{
	if (!IsValid(BoundQuestComponent))
	{
		BP_OnRefreshQuestTracker(
			TArray<FTDQuestViewData>());
		return;
	}

	BP_OnRefreshQuestTracker(
		BoundQuestComponent
			->GetQuestTrackerViews());
}
