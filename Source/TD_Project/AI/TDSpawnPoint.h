#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDSpawnPoint.generated.h"

class ATDEnemyBase;

/**
 * 몬스터 1마리의 자리. 레벨에 배치하면 서버가 여기에 몬스터를 스폰하고,
 * 죽으면 RespawnDelay 뒤에 같은 자리에 다시 스폰한다.
 *
 * 몬스터를 직접 배치하는 대신 이걸 쓰는 이유: 직접 배치한 몬스터는 죽으면 영영 사라진다.
 */
UCLASS()
class TD_PROJECT_API ATDSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ATDSpawnPoint();

	/** 몬스터를 스폰하고 사망 구독을 건다. 이미 살아 있으면 아무 일도 하지 않는다. */
	void SpawnMonster();

	/** 예약된 재스폰을 취소한다. 레벨 이동 시 서브시스템이 부른다. */
	void CancelRespawn();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 스폰할 몬스터 클래스. BP_SpawnPoint 에서 BP_Goblin 등을 지정한다(에셋 참조는 BP 몫). */
	UPROPERTY(EditAnywhere, Category = "TD|Spawn")
	TSubclassOf<ATDEnemyBase> MonsterClass;

	/**
	 * DT_MonsterDefinition 행 이름. 비워두면 몬스터 BP 의 기본값을 그대로 쓴다.
	 * 지정하면 같은 BP 로 다른 몬스터(팔레트 교체)를 만들 수 있다.
	 */
	UPROPERTY(EditAnywhere, Category = "TD|Spawn")
	FName MonsterId;

	/** 스폰할 레벨. 포인트마다 다르게 두면 같은 사냥터에 저레벨·고레벨 존이 생긴다. */
	UPROPERTY(EditAnywhere, Category = "TD|Spawn", meta = (ClampMin = "1"))
	int32 Level = 1;

	/** 사망 후 재스폰까지 걸리는 시간(초). */
	UPROPERTY(EditAnywhere, Category = "TD|Spawn", meta = (ClampMin = "0"))
	float RespawnDelay = 10.f;

private:
	UFUNCTION()
	void HandleMonsterDeath();

	/** 지금 이 자리에 살아 있는 몬스터. 없으면 nullptr. */
	UPROPERTY()
	TObjectPtr<ATDEnemyBase> CurrentMonster;

	FTimerHandle RespawnTimerHandle;
};