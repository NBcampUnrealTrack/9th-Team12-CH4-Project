#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TDSpawnPoint.generated.h"

class ATDEnemyBase;

/**
 * 한 그룹이 쓰는 몬스터 한 마리의 자리.
 *
 * 인스턴싱을 켜면 그룹(파티 또는 혼자인 사람)마다 이 자리에 한 마리씩 따로 생긴다.
 * 서로의 몬스터는 보이지도 않고 때릴 수도 없다.
 */
USTRUCT()
struct FTDSpawnSlot
{
	GENERATED_BODY()

	/** 지금 이 자리에 살아 있는 몬스터. 없으면 nullptr. */
	UPROPERTY()
	TObjectPtr<ATDEnemyBase> Monster;

	/** 다시 스폰해도 되는 시각(서버 월드시간). 죽은 것을 확인한 순간 정해진다. */
	float RespawnTime = 0.f;

	/** 이 그룹의 누군가가 마지막으로 활성 반경 안에 있던 시각. 정리 판단에 쓴다. */
	float LastSeenTime = 0.f;
};

/**
 * 몬스터 1마리의 자리. 레벨에 배치하면 서버가 여기에 몬스터를 스폰하고,
 * 죽으면 RespawnDelay 뒤에 같은 자리에 다시 스폰한다.
 *
 * 몬스터를 직접 배치하는 대신 이걸 쓰는 이유: 직접 배치한 몬스터는 죽으면 영영 사라진다.
 *
 * ── 그룹별 인스턴싱 ──
 * bInstancePerGroup 을 켜면 "자리 하나 = 몬스터 하나" 가 아니라 **"자리 하나 = 그룹마다 하나"**
 * 가 된다. 파티는 같은 몬스터를 함께 잡고, 파티가 아닌 사람은 자기 몬스터만 본다.
 *
 * 몬스터는 활성 반경 안에 그 그룹 사람이 있을 때만 존재한다. 아무도 없는 사냥터에는
 * 한 마리도 없으므로, 켜기 전보다 오히려 가벼워지는 경우가 많다.
 */
UCLASS()
class TD_PROJECT_API ATDSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ATDSpawnPoint();

	/**
	 * 공용 몬스터(그룹 없음)를 지금 스폰한다. 이미 있으면 아무 일도 하지 않는다.
	 *
	 * 인스턴싱을 켠 자리에서는 쓰지 않는다 — 그쪽은 누가 왔는지에 따라 주기 검사가 정한다.
	 */
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

	// ── 그룹별 인스턴싱 ───────────────────────────────────

	/**
	 * 그룹(파티 또는 개인)마다 몬스터를 따로 둘 것인가. **사냥터는 켜고 보스방은 끈다.**
	 *
	 * 보스는 이미 방 배정으로 나뉘어 있다. 여기까지 켜면 같은 방에서도 파티마다 보스가
	 * 따로 생겨 규칙이 두 겹이 된다.
	 */
	UPROPERTY(EditAnywhere, Category = "TD|Spawn|Instancing")
	bool bInstancePerGroup = true;

	/**
	 * 이 반경 안에 사람이 있는 그룹에만 몬스터를 만든다.
	 *
	 * 너무 좁으면 달려오는 동안 눈앞에서 몬스터가 생겨나는 것이 보이고,
	 * 너무 넓으면 지나가기만 해도 사냥터 전체가 깨어난다.
	 */
	UPROPERTY(EditAnywhere, Category = "TD|Spawn|Instancing",
		meta = (ClampMin = "100", EditCondition = "bInstancePerGroup"))
	float ActivationRadius = 6000.f;

	/**
	 * 그룹이 반경을 벗어난 뒤 몬스터를 지우기까지 기다리는 시간(초).
	 *
	 * 0 으로 두면 잠깐 물러서서 체력을 회복하는 사이에 쫓아오던 몬스터가 사라진다.
	 */
	UPROPERTY(EditAnywhere, Category = "TD|Spawn|Instancing",
		meta = (ClampMin = "0", EditCondition = "bInstancePerGroup"))
	float GroupExitGrace = 30.f;

	/** 누가 근처에 있는지 다시 확인하는 간격(초). */
	UPROPERTY(EditAnywhere, Category = "TD|Spawn|Instancing", meta = (ClampMin = "0.1"))
	float UpdateInterval = 2.f;

private:
	/**
	 * 자리 전체를 한 번 살핀다. 주기적으로 돈다.
	 *
	 * 스폰·재스폰·정리를 모두 여기서 판단한다. 죽음 알림과 재스폰 타이머를 따로 두지 않는 이유는
	 * 그룹이 늘고 줄 때마다 타이머를 붙였다 떼는 것보다 한 곳에서 보는 편이 새는 곳이 적어서다.
	 */
	void UpdateSlots();

	/** 지금 이 자리를 쓰는 그룹들. 인스턴싱을 끈 자리는 빈 키 하나(공용)만 돌려준다. */
	TSet<FGuid> GatherActiveGroups() const;

	/** 한 그룹의 몬스터를 만든다. */
	void SpawnForGroup(const FGuid& GroupId, FTDSpawnSlot& Slot);

	/**
	 * 이 몬스터가 누구를 통과할지 맞춘다. 같은 그룹은 그대로 막고, 남은 서로 통과한다.
	 *
	 * 주기 검사에서 부르므로 파티 가입·탈퇴나 새로 온 사람도 다음 검사에서 반영된다.
	 * 이미 PlayerArray 를 훑는 자리라 추가 비용이 거의 없다.
	 */
	void RefreshMoveIgnores(ATDEnemyBase* Monster, const FGuid& GroupId);

	/** 그룹별 자리. 인스턴싱을 끄면 빈 키 하나만 쓴다. */
	UPROPERTY()
	TMap<FGuid, FTDSpawnSlot> Slots;

	FTimerHandle UpdateTimerHandle;
};
