#pragma once

#include "CoreMinimal.h"
#include "Character/TDEnemyBase.h"
#include "Combat/TDBossPattern.h"
#include "Stats/TDStatTypes.h"
#include "TDBossCharacter.generated.h"

class UCameraShakeBase;
class UNiagaraComponent;
class ATDBossProjectile;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDOnBossEvent, ETDBossEvent, Event, int32, Param);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FTDOnBossPatternTelegraph,
	int32, PatternIndex, FVector, Center, FVector, Direction, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOnBossPhaseChanged, int32, NewPhase);
DECLARE_MULTICAST_DELEGATE_OneParam(FTDOnBossPatternFinished, int32 /*PatternIndex*/);

/**
 * 보스 몬스터. 패턴 엔진(선딜→타격→후딜)·페이즈·분노·입장·소환·리시 귀환·화면 흔들림·VFX.
 *
 * 두뇌(BT)는 "무엇을 할지"만 정하고 StartPattern 을 부른다. 이동은 훅에서 이 클래스가 처리한다.
 * 데미지는 전부 CombatStatics::ApplyDamage, 히트 방송은 CombatComponent::NotifyHit.
 * 전투 중인 보스는 GameState.ActiveBoss 로 HUD 에 알려진다.
 */
UCLASS()
class TD_PROJECT_API ATDBossCharacter : public ATDEnemyBase
{
	GENERATED_BODY()

public:
	ATDBossCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	// ── 서버 API: BT·치트가 부른다 ────────────────────────

	void BeginFight(ATDCharacterBase* FirstTarget);

	/** 리시: 집으로 걸어 돌아가 풀피·페이즈 1·쫄 정리. bInstant 면 순간이동(치트·사망 정리용). */
	void ResetFight(bool bInstant = false);

	bool StartPattern(int32 PatternIndex);
	void CancelPattern();
	int32 ChoosePattern(float DistanceToTarget) const;
	void SetPatternTarget(ATDCharacterBase* Target);
	ATDCharacterBase* FindEnemy(bool bFarthest, float MaxRange) const;
	void EnterPhase2();
	void TriggerEnrage();
	void SummonMinions();

	// ── 조회 ─────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	bool IsBusy() const;

	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	bool IsFightActive() const { return bFightActive; }

	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	bool IsReturning() const { return bReturning; }

	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	int32 GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	bool IsEnraged() const { return bEnraged; }

	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	int32 GetPatternCount() const { return Patterns.Num(); }

	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	bool GetPatternSpec(int32 PatternIndex, FTDBossPatternSpec& OutSpec) const;

	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	ETDBossPatternPhase GetCurrentPatternPhase() const { return CurrentPatternPhase; }

	/** 테이블의 DisplayName. 보스 체력바·타이틀 카드용. 행이 없으면 MonsterId. */
	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	FText GetDisplayName() const;

	FVector GetHomeLocation() const { return HomeLocation; }
	float GetLeashRadius() const { return LeashRadius; }
	ATDCharacterBase* GetPatternTarget() const { return PatternTarget.Get(); }

	// ── 델리게이트 ────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "TD|Boss")
	FTDOnBossEvent OnBossEvent;

	UPROPERTY(BlueprintAssignable, Category = "TD|Boss")
	FTDOnBossPatternTelegraph OnPatternTelegraph;

	UPROPERTY(BlueprintAssignable, Category = "TD|Boss")
	FTDOnBossPhaseChanged OnPhaseChanged;

	FTDOnBossPatternFinished OnPatternFinished;

	// ── 슈퍼아머·피해 규칙 ─────────────────────────────────

	virtual bool CanBeStaggered() const override { return false; }
	virtual bool IsInvulnerable() const override;
	virtual float GetIncomingDamageMultiplier() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleDeath() override;

	// ── 이동 훅 ─────────────────────────────────────────

	virtual void OnTelegraphBegin(const FTDBossPatternSpec& Spec);
	virtual void OnMotionBegin(const FTDBossPatternSpec& Spec);
	virtual void OnMotionEnd(const FTDBossPatternSpec& Spec);
	virtual FVector GetStrikeCenter(const FTDBossPatternSpec& Spec) const;

	// ── 패턴 명세 ────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns")
	TArray<FTDBossPatternSpec> Patterns;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns")
	bool bAvoidRepeatingPattern = true;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns")
	bool bDrawDebugHits = true;

	// ── 이동 파라미터 ─────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float DashSpeed = 1400.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion")
	TSubclassOf<ATDBossProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float ProjectileSpeed = 1200.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float ProjectileMuzzleForward = 120.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion")
	float ProjectileMuzzleHeight = 0.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "1"))
	int32 ProjectileCountPhase2 = 3;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float ProjectileSpreadAngle = 20.f;

	// ── 페이즈 ───────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Phase", meta = (ClampMin = "0", ClampMax = "1"))
	float Phase2HealthRatio = 0.5f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Phase", meta = (ClampMin = "0.1", ClampMax = "1"))
	float Phase2TelegraphScale = 0.7f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Phase", meta = (ClampMin = "0.1", ClampMax = "1"))
	float Phase2RecoveryScale = 0.8f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Phase", meta = (ClampMin = "0"))
	float Phase2TransitionDuration = 2.f;

	/** 페이즈 2 공격력 증가(Increased). 0.2 = +20%. Source.Boss 스탯 소스로 건다. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Phase", meta = (ClampMin = "0"))
	float Phase2DamageBonus = 0.2f;

	// ── 분노 ─────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Enrage", meta = (ClampMin = "0"))
	float EnrageAfterSeconds = 240.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Enrage", meta = (ClampMin = "0.1", ClampMax = "1"))
	float EnrageTelegraphScale = 0.5f;

	/** 분노 공격력 증가. 페이즈 2 와 합산된다. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Enrage", meta = (ClampMin = "0"))
	float EnrageDamageBonus = 0.3f;

	// ── 전투 ─────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "0"))
	float EntranceDuration = 2.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "1"))
	float RecoveryIncomingDamageMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "0"))
	float LeashRadius = 2500.f;

	/** 귀환 도착 판정 거리. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "0"))
	float ReturnArriveDistance = 150.f;

	/** 귀환이 이 시간 안에 못 끝나면(길 막힘) 순간이동으로 마무리. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "1"))
	float ReturnTimeout = 8.f;

	// ── 소환 ─────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Summon")
	TSubclassOf<ATDEnemyBase> MinionClass;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Summon")
	FName MinionId;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Summon", meta = (ClampMin = "0"))
	int32 MinionCount = 2;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Summon", meta = (ClampMin = "0"))
	float MinionSpawnRadius = 400.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Summon")
	bool bSummonOnPhase2 = true;

	// ── 화면 흔들림 ───────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Feedback")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Feedback", meta = (ClampMin = "0"))
	float ShakeInnerRadius = 600.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Feedback", meta = (ClampMin = "0"))
	float ShakeOuterRadius = 2500.f;

	// ── 방송 ─────────────────────────────────────────────

	UFUNCTION(NetMulticast, Reliable)
	void MulticastBossEvent(ETDBossEvent Event, int32 Param);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPatternTelegraph(int32 PatternIndex, FVector Center, FVector Direction, float Duration);

	/** 타격 시작. OnBossEvent(PatternStrike) 발화 + 예고 VFX 제거 + 타격 VFX 스폰. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPatternStrike(int32 PatternIndex, FVector Center, FVector Direction);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCameraShake(FVector Epicenter, float Scale);

	UFUNCTION()
	void OnRep_Phase();

	// ── 패턴 진행 (서버) ──────────────────────────────────

	void EnterStrike();
	void DoStrikeHit();
	void EnterRecovery();
	void FinishPattern();
	float ScaledTelegraph(float Base) const;
	float ScaledRecovery(float Base) const;

	// ── 이동 구현 (서버) ──────────────────────────────────

	void StartDash();
	void TickDash(float DeltaSeconds);
	void EndDash(bool bHitWall);
	void StartBurrow(const FTDBossPatternSpec& Spec);
	void TickBurrow(float DeltaSeconds);
	void EndBurrow();
	void FireProjectiles(const FTDBossPatternSpec& Spec);
	FVector GetFacing() const;

	// ── 귀환·강화·등록 (서버) ─────────────────────────────

	void StartReturnHome();
	void TickReturn();
	void FinishReturnHome();
	void ApplyBossBuff();
	void ClearBossBuff();
	void RegisterActiveBoss(bool bActive);

	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void EndEntrance();
	void EndPhase2Transition();
	void DestroyMinions();
	void ClearFightTimers();
	void PlayShakeLocally(const FVector& Epicenter, float Scale) const;

	// ── VFX (전 머신) ─────────────────────────────────────

	void ClearTelegraphVFX();

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	int32 Phase = 1;

	UPROPERTY(Replicated)
	bool bEnraged = false;

	FVector PatternTargetLocation = FVector::ZeroVector;
	TWeakObjectPtr<ATDCharacterBase> PatternTarget;
	ETDBossPatternPhase CurrentPatternPhase = ETDBossPatternPhase::None;
	int32 CurrentPattern = INDEX_NONE;

private:
	bool bFightActive = false;
	bool bInEntrance = false;
	bool bInPhaseTransition = false;
	bool bReturning = false;

	bool bDashing = false;
	FVector DashDirection = FVector::ForwardVector;

	bool bBurrowed = false;
	FVector BurrowFrom = FVector::ZeroVector;
	FVector BurrowTo = FVector::ZeroVector;
	float BurrowElapsed = 0.f;
	float BurrowDuration = 1.f;

	int32 LastPattern = INDEX_NONE;
	TArray<float> PatternReadyTime;
	TSet<TWeakObjectPtr<AActor>> HitThisStrike;

	FVector HomeLocation = FVector::ZeroVector;
	float FightStartTime = 0.f;

	TArray<TWeakObjectPtr<ATDEnemyBase>> Minions;
	FTDStatSourceHandle BossBuffHandle;

	/** 선딜 동안 떠 있는 예고 이펙트. 머신마다 자기 것. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> TelegraphVFXComponent;

	FTimerHandle PhaseTimerHandle;
	FTimerHandle StrikeTickHandle;
	FTimerHandle EntranceTimerHandle;
	FTimerHandle EnrageTimerHandle;
	FTimerHandle TransitionTimerHandle;
	FTimerHandle ReturnTimeoutHandle;

	FDelegateHandle HealthChangedHandle;
};