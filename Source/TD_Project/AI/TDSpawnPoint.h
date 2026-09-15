#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
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

	/**
	 * 존 초기화. 죽어 있으면 기다리지 않고 지금 다시 스폰하고,
	 * 살아 있는 보스는 전투를 처음으로 되돌린다(ATDBossCharacter::ResetFight).
	 *
	 * 보스방이 빈 뒤 다음 파티가 들어갈 때 UTDSpawnSubsystem::ResetZone 이 부른다.
	 */
	void ResetForZone();

	FGameplayTag GetZoneId() const { return ZoneId; }

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

	/**
	 * 죽으면 RespawnDelay 뒤에 스스로 다시 나오는가.
	 *
	 * **보스방에서는 끈다.** 켜 두면 파티가 방 안에 머무르며 보스를 계속 다시 잡을 수 있다.
	 * 끄면 그 존이 비었다가 다음 파티가 들어올 때(ResetForZone)만 되살아난다.
	 */
	UPROPERTY(EditAnywhere, Category = "TD|Spawn")
	bool bAutoRespawn = true;

	/** 사망 후 재스폰까지 걸리는 시간(초). */
	UPROPERTY(EditAnywhere, Category = "TD|Spawn", meta = (ClampMin = "0", EditCondition = "bAutoRespawn"))
	float RespawnDelay = 10.f;

	/**
	 * 이 자리가 속한 존. UTDSpawnSubsystem::ResetZone 이 이 값으로 포인트를 고른다.
	 *
	 * 사냥터 몬스터는 비워 두어도 된다 — 존 단위 명령에 걸리지 않을 뿐이다.
	 * **보스방 보스는 반드시 방 태그를 넣는다**(Zone.Region1.Boss.Room01).
	 * 비워 두면 방은 배정되는데 보스가 초기화되지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "TD|Spawn")
	FGameplayTag ZoneId;

private:
	UFUNCTION()
	void HandleMonsterDeath();

	/** 지금 이 자리에 살아 있는 몬스터. 없으면 nullptr. */
	UPROPERTY()
	TObjectPtr<ATDEnemyBase> CurrentMonster;

	FTimerHandle RespawnTimerHandle;
};