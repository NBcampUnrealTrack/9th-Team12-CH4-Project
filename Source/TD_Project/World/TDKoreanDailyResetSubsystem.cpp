#include "World/TDKoreanDailyResetSubsystem.h"

#include "Engine/World.h"
#include "TimerManager.h"

int32 UTDKoreanDailyResetSubsystem::GetCurrentKstDayKey()
{
	const FDateTime KstNow =
		FDateTime::UtcNow()
		+ FTimespan::FromHours(9.0);

	return KstNow.GetYear() * 10000
		+ KstNow.GetMonth() * 100
		+ KstNow.GetDay();
}

double UTDKoreanDailyResetSubsystem::
GetSecondsUntilNextKstMidnight()
{
	const FDateTime KstNow =
		FDateTime::UtcNow()
		+ FTimespan::FromHours(9.0);

	const FDateTime TodayMidnight(
		KstNow.GetYear(),
		KstNow.GetMonth(),
		KstNow.GetDay());

	const FDateTime NextMidnight =
		TodayMidnight
		+ FTimespan::FromDays(1.0);

	return FMath::Max(
		1.0,
		(NextMidnight - KstNow)
			.GetTotalSeconds());
}

void UTDKoreanDailyResetSubsystem::OnWorldBeginPlay(
	UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	ScheduleNextMidnight();
}

void UTDKoreanDailyResetSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			MidnightTimerHandle);
	}

	OnKoreanDayChanged.Clear();

	Super::Deinitialize();
}

void UTDKoreanDailyResetSubsystem::
ScheduleNextMidnight()
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(
		MidnightTimerHandle);

	World->GetTimerManager().SetTimer(
		MidnightTimerHandle,
		this,
		&UTDKoreanDailyResetSubsystem::
			HandleMidnight,
		static_cast<float>(
			GetSecondsUntilNextKstMidnight()),
		false);
}

void UTDKoreanDailyResetSubsystem::HandleMidnight()
{
	OnKoreanDayChanged.Broadcast();
	ScheduleNextMidnight();
}