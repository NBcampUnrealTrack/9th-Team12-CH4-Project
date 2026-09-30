#include "AI/TDSpawnSubsystem.h"

#include "AI/TDSpawnPoint.h"

void UTDSpawnSubsystem::RegisterSpawnPoint(ATDSpawnPoint* Point)
{
	if (Point != nullptr)
	{
		SpawnPoints.AddUnique(Point);
	}
}

void UTDSpawnSubsystem::UnregisterSpawnPoint(ATDSpawnPoint* Point)
{
	SpawnPoints.Remove(Point);
}

void UTDSpawnSubsystem::ClearAll()
{
	for (const TWeakObjectPtr<ATDSpawnPoint>& Point : SpawnPoints)
	{
		if (ATDSpawnPoint* Valid = Point.Get())
		{
			Valid->CancelRespawn();
		}
	}
}

void UTDSpawnSubsystem::ResetZone(FGameplayTag ZoneId)
{
	if (!ZoneId.IsValid())
	{
		return;
	}

	int32 ResetCount = 0;

	for (const TWeakObjectPtr<ATDSpawnPoint>& Point : SpawnPoints)
	{
		ATDSpawnPoint* Valid = Point.Get();
		if (Valid != nullptr && Valid->GetZoneId() == ZoneId)
		{
			Valid->ResetForZone();
			++ResetCount;
		}
	}

	// 방은 배정됐는데 보스가 안 나오면 원인을 찾기 어렵다. 포인트가 하나도 없으면 알린다 —
	// 보스를 스폰포인트 없이 직접 배치했거나 ZoneId 를 비워 둔 경우다.
	if (ResetCount == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("존 초기화: '%s' 에 속한 스폰포인트가 없다. 보스를 스폰포인트로 배치하고 ZoneId 를 방 태그로 지정할 것."),
			*ZoneId.ToString());
	}
}