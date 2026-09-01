#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TDSpawnSubsystem.generated.h"

class ATDSpawnPoint;

/**
 * 스폰 포인트의 명부. 월드마다 하나씩 자동 생성된다.
 *
 * 실제 스폰·리젠은 각 포인트가 스스로 하고, 여기는 전역 통제만 맡는다 —
 * 지금은 ClearAll(레벨 이동 시 일괄 정지)이 전부지만, 나중에
 * "이 존의 몬스터 전부 리젠" 같은 명령이 붙을 자리다.
 */
UCLASS()
class TD_PROJECT_API UTDSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterSpawnPoint(ATDSpawnPoint* Point);
	void UnregisterSpawnPoint(ATDSpawnPoint* Point);

	/** 모든 포인트의 재스폰 예약을 취소한다. 레벨을 떠날 때 부른다. */
	void ClearAll();

	int32 GetSpawnPointCount() const { return SpawnPoints.Num(); }

private:
	/** 포인트가 먼저 파괴돼도 안전하도록 약한 참조로 든다. */
	TArray<TWeakObjectPtr<ATDSpawnPoint>> SpawnPoints;
};