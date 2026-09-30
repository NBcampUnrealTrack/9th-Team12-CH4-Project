#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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

	/**
	 * 한 존의 포인트를 전부 초기화한다(ATDSpawnPoint::ResetForZone).
	 *
	 * 보스방이 비었다가 다음 파티가 들어갈 때 GameMode 가 부른다.
	 * ZoneId 가 **정확히** 같은 포인트만 고른다 — 방 묶음(Zone.Region1.Boss)으로 부르면
	 * 남이 싸우고 있는 다른 방의 보스까지 되돌아간다.
	 */
	void ResetZone(FGameplayTag ZoneId);

	int32 GetSpawnPointCount() const { return SpawnPoints.Num(); }

private:
	/** 포인트가 먼저 파괴돼도 안전하도록 약한 참조로 든다. */
	TArray<TWeakObjectPtr<ATDSpawnPoint>> SpawnPoints;
};