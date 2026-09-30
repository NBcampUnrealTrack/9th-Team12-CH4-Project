#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TDKoreanDailyResetSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FTDOnKoreanDayChanged);

UCLASS()
class TD_PROJECT_API UTDKoreanDailyResetSubsystem
	: public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	static int32 GetCurrentKstDayKey();
	static double GetSecondsUntilNextKstMidnight();

	FTDOnKoreanDayChanged OnKoreanDayChanged;

private:
	void ScheduleNextMidnight();
	void HandleMidnight();

	FTimerHandle MidnightTimerHandle;
};