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