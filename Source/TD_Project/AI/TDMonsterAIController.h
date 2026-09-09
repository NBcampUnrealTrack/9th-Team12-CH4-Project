#pragma once

#include "AIController.h"
#include "CoreMinimal.h"
#include "TDMonsterAIController.generated.h"

class ATDCharacterBase;
class ATDEnemyBase;

/** 필드 몬스터의 행동 상태. 항상 이 중 하나다. */
UENUM()
enum class ETDMonsterAIState : uint8
{
	Idle,     // 제자리 대기. 시간이 지나면 배회로.
	Wander,   // 스폰 주변 랜덤 지점으로 이동 중.
	Sense,    // 적 발견 직후의 반응 모션. 잠깐 정지 — 플레이어가 도망칠 틈이다.
	Combat    // 추적·공격. 거리로 세부 행동이 갈린다.
};

/**
 * 필드 몬스터의 두뇌. 대기 ↔ 배회 → (발견) 인지 → 전투.
 *
 * 명시적 상태 머신이다. 배회·인지처럼 "시간이 걸리는 상태"가 생기면서
 * if문 암묵 FSM 으로는 "지금 뭐 하는 중 + 언제 끝나는지"를 표현할 수 없게 됐다.
 *
 * 공격은 CombatComponent 에 위임한다. 서버 전용.
 */
UCLASS()
class TD_PROJECT_API ATDMonsterAIController : public AAIController
{
	GENERATED_BODY()

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	// ── 전투 파라미터 (기존과 동일) ──────────────────────

	UPROPERTY(EditDefaultsOnly, Category = "TD|AI", meta = (ClampMin = "0"))
	float AggroRadius = 800.f;

	UPROPERTY(EditDefaultsOnly, Category = "TD|AI", meta = (ClampMin = "0"))
	float LoseAggroRadius = 1400.f;

	UPROPERTY(EditDefaultsOnly, Category = "TD|AI", meta = (ClampMin = "0"))
	float AttackRange = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "TD|AI", meta = (ClampMin = "0.05"))
	float ThinkInterval = 0.25f;

	// ── 배회·인지 파라미터 ────────────────────────────────

	/** 스폰 지점에서 이 반경 안의 지점으로만 배회한다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|AI|Wander", meta = (ClampMin = "0"))
	float WanderRadius = 500.f;

	/** 배회 후 다음 배회까지 서 있는 시간 범위(초). */
	UPROPERTY(EditDefaultsOnly, Category = "TD|AI|Wander", meta = (ClampMin = "0"))
	float IdleTimeMin = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "TD|AI|Wander", meta = (ClampMin = "0"))
	float IdleTimeMax = 4.f;

	/** 배회 이동의 포기 시한(초). 경로가 막혀도 이 시간이 지나면 대기로 돌아간다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|AI|Wander", meta = (ClampMin = "1"))
	float WanderTimeLimit = 6.f;

	/** 적 발견 후 굳어 있는 시간(초). 발견 모션 길이에 맞춘다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|AI|Wander", meta = (ClampMin = "0"))
	float SenseDuration = 0.8f;

private:
	void Think();

	// 상태 진입 함수들. 상태 전환은 반드시 이들을 거친다 — 진입 시 해야 할 일을 한 곳에 모은다.
	void EnterIdle();
	void EnterWander();
	void EnterSense(ATDCharacterBase* Found);
	void EnterCombat();

	/** 전투 상태의 매 판단. 기존 추적/공격 로직이 그대로 여기 있다. */
	void TickCombat(ATDCharacterBase* Self);

	ATDCharacterBase* FindNearestEnemy() const;

	UFUNCTION()
	void HandlePawnDeath();

	ETDMonsterAIState State = ETDMonsterAIState::Idle;

	/** 현재 상태가 끝나는 시각(서버 월드시간). Idle/Wander/Sense 가 쓴다. */
	float StateEndTime = 0.f;

	/** 스폰 지점. 배회의 중심이자, 나중에 귀환(Leash)의 목적지가 된다. */
	FVector HomeLocation = FVector::ZeroVector;

	TWeakObjectPtr<ATDCharacterBase> AggroTarget;

	FTimerHandle ThinkTimerHandle;
	
	/** 맞았다. 어그로를 끌고, 경직 동안 멈춘다. 서버 전용(OnDamagedServer 구독). */
	void HandlePawnDamaged(AActor* Attacker, float Damage, bool bCritical);
};