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
 * 보스 몬스터. 패턴 엔진(선딜→타격→후딜)·페이즈·분노·입장·소환·리시 귀환·흔들림·VFX·몸 연출.
 * 두뇌(BT)가 "무엇을"을 정하면 이 클래스가 "어떻게"를 한다. 이동·회전·솟구침은 서버 Tick.
 */
UCLASS()
class TD_PROJECT_API ATDBossCharacter : public ATDEnemyBase
{
	GENERATED_BODY()

public:
	ATDBossCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	// ── 서버 API ─────────────────────────────────────────
	void BeginFight(ATDCharacterBase* FirstTarget);
	void ResetFight(bool bInstant = false);
	bool StartPattern(int32 PatternIndex);
	void CancelPattern();
	int32 ChoosePattern(float DistanceToTarget) const;
	void SetPatternTarget(ATDCharacterBase* Target);
	ATDCharacterBase* FindEnemy(bool bFarthest, float MaxRange) const;
	void EnterPhase2();
	void TriggerEnrage();
	void SummonMinions();
	/** 지금 거리에서 쓸 수 있는 패턴이 하나라도 있나. 쉼·쿨·거리 조건을 본다(랜덤은 안 굴림). */
	bool HasReadyPattern(float DistanceToTarget) const;

	// ── 조회 ─────────────────────────────────────────────
	UFUNCTION(BlueprintPure, Category = "TD|Boss") bool IsBusy() const;
	UFUNCTION(BlueprintPure, Category = "TD|Boss") bool IsFightActive() const { return bFightActive; }
	UFUNCTION(BlueprintPure, Category = "TD|Boss") bool IsReturning() const { return bReturning; }
	UFUNCTION(BlueprintPure, Category = "TD|Boss") int32 GetPhase() const { return Phase; }
	UFUNCTION(BlueprintPure, Category = "TD|Boss") bool IsEnraged() const { return bEnraged; }
	UFUNCTION(BlueprintPure, Category = "TD|Boss") int32 GetPatternCount() const { return Patterns.Num(); }
	UFUNCTION(BlueprintPure, Category = "TD|Boss") bool GetPatternSpec(int32 PatternIndex, FTDBossPatternSpec& OutSpec) const;
	UFUNCTION(BlueprintPure, Category = "TD|Boss") ETDBossPatternPhase GetCurrentPatternPhase() const { return CurrentPatternPhase; }
	UFUNCTION(BlueprintPure, Category = "TD|Boss") FText GetDisplayName() const;

	FVector GetHomeLocation() const { return HomeLocation; }
	/** 내비 목적지용 집 좌표(발 높이). 캡슐 중심을 주면 키 큰 보스는 투영에 실패해 Move To 가 즉시 실패한다. */
	FVector GetHomeNavLocation() const;
	float GetLeashRadius() const { return LeashRadius; }
	ATDCharacterBase* GetPatternTarget() const { return PatternTarget.Get(); }

	// ── 델리게이트 ────────────────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "TD|Boss") FTDOnBossEvent OnBossEvent;
	UPROPERTY(BlueprintAssignable, Category = "TD|Boss") FTDOnBossPatternTelegraph OnPatternTelegraph;
	UPROPERTY(BlueprintAssignable, Category = "TD|Boss") FTDOnBossPhaseChanged OnPhaseChanged;
	FTDOnBossPatternFinished OnPatternFinished;

	virtual bool CanBeStaggered() const override { return false; }
	virtual bool IsInvulnerable() const override;
	virtual float GetIncomingDamageMultiplier() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleDeath() override;

	virtual void OnTelegraphBegin(const FTDBossPatternSpec& Spec);
	virtual void OnMotionBegin(const FTDBossPatternSpec& Spec);
	virtual void OnMotionEnd(const FTDBossPatternSpec& Spec);
	virtual FVector GetStrikeCenter(const FTDBossPatternSpec& Spec) const;

	// ── 패턴 명세 ────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns") TArray<FTDBossPatternSpec> Patterns;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns") bool bAvoidRepeatingPattern = true;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns") bool bDrawDebugHits = true;

	/** 패턴과 패턴 사이 쉬는 시간(초) 범위. 이 동안 걷고 재배치한다 — 없으면 쉴 새 없이 두들긴다. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns", meta = (ClampMin = "0"))
	float PatternGapMin = 1.0f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns", meta = (ClampMin = "0"))
	float PatternGapMax = 2.0f;

	// ── 몸 연출 ──────────────────────────────────────────
	/** 선딜 동안 몸이 판정 방향으로 도는 속도(도/초). */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float TurnSpeedDegrees = 540.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float DashSpeed = 1400.f;

	/** 돌진 중 몸에 닿은 캐릭터를 밀어내는 속도. 겹치지 않고 튕겨 나간다. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float DashShove = 900.f;

	/** 솟구침: 이 깊이(cm)에서 시작해 */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float EmergeDepth = 500.f;

	/** 지면 위 이 높이까지 튀어오른 뒤 내려온다. 정점에서 판정. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float EmergeHeight = 250.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0.05"))
	float EmergeRiseTime = 0.25f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0.05"))
	float EmergeFallTime = 0.2f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion") TSubclassOf<ATDBossProjectile> ProjectileClass;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0")) float ProjectileSpeed = 1200.f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0")) float ProjectileMuzzleForward = 120.f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion") float ProjectileMuzzleHeight = 0.f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "1")) int32 ProjectileCountPhase2 = 3;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0")) float ProjectileSpreadAngle = 20.f;

	// ── 페이즈·분노·전투·소환·흔들림 (기존) ──────────────
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Phase", meta = (ClampMin = "0", ClampMax = "1")) float Phase2HealthRatio = 0.5f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Phase", meta = (ClampMin = "0.1", ClampMax = "1")) float Phase2TelegraphScale = 0.7f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Phase", meta = (ClampMin = "0.1", ClampMax = "1")) float Phase2RecoveryScale = 0.8f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Phase", meta = (ClampMin = "0")) float Phase2TransitionDuration = 2.f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Phase", meta = (ClampMin = "0")) float Phase2DamageBonus = 0.2f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Enrage", meta = (ClampMin = "0")) float EnrageAfterSeconds = 240.f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Enrage", meta = (ClampMin = "0.1", ClampMax = "1")) float EnrageTelegraphScale = 0.5f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Enrage", meta = (ClampMin = "0")) float EnrageDamageBonus = 0.3f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "0")) float EntranceDuration = 2.f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "1")) float RecoveryIncomingDamageMultiplier = 1.5f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "0")) float LeashRadius = 2500.f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "0")) float ReturnArriveDistance = 150.f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "1")) float ReturnTimeout = 12.f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Summon") TSubclassOf<ATDEnemyBase> MinionClass;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Summon") FName MinionId;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Summon", meta = (ClampMin = "0")) int32 MinionCount = 2;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Summon", meta = (ClampMin = "0")) float MinionSpawnRadius = 400.f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Summon") bool bSummonOnPhase2 = true;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Feedback") TSubclassOf<UCameraShakeBase> CameraShakeClass;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Feedback", meta = (ClampMin = "0")) float ShakeInnerRadius = 600.f;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Feedback", meta = (ClampMin = "0")) float ShakeOuterRadius = 2500.f;

	// ── 방송 ─────────────────────────────────────────────
	UFUNCTION(NetMulticast, Reliable) void MulticastBossEvent(ETDBossEvent Event, int32 Param);
	UFUNCTION(NetMulticast, Reliable) void MulticastPatternTelegraph(int32 PatternIndex, FVector Center, FVector Direction, float Duration);
	UFUNCTION(NetMulticast, Reliable) void MulticastPatternStrike(int32 PatternIndex, FVector Center, FVector Direction);
	UFUNCTION(NetMulticast, Unreliable) void MulticastCameraShake(FVector Epicenter, float Scale);
	UFUNCTION() void OnRep_Phase();

	// ── 패턴 진행 ────────────────────────────────────────
	void EnterStrike();
	void DoStrikeHit();
	void EnterRecovery();
	void FinishPattern();
	float ScaledTelegraph(float Base) const;
	float ScaledRecovery(float Base) const;
	void ApplyKnockback(ATDCharacterBase* Target, const FVector& Direction, float Strength, float UpRatio) const;

	// ── 이동·몸 ──────────────────────────────────────────
	void TickTurn(float DeltaSeconds);
	void StartDash();
	void TickDash(float DeltaSeconds);
	void EndDash(bool bHitWall);
	void StartBurrow(const FTDBossPatternSpec& Spec);
	void TickBurrow(float DeltaSeconds);
	void StartEmerge();
	void TickEmerge(float DeltaSeconds);
	void FinishEmerge();
	void AbortBurrow();
	void FireProjectiles(const FTDBossPatternSpec& Spec);
	FVector GetFacing() const;
	void SetMovementFrozen(bool bFrozen);

	// ── 귀환·강화·등록 ──────────────────────────────────
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
	void ClearTelegraphVFX();

	UPROPERTY(ReplicatedUsing = OnRep_Phase) int32 Phase = 1;
	UPROPERTY(Replicated) bool bEnraged = false;

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

	// 잠수: 땅속 이동 → 솟구침(상승·하강) → 착지
	bool bBurrowed = false;
	bool bEmerging = false;
	FVector BurrowFrom = FVector::ZeroVector;
	FVector BurrowTo = FVector::ZeroVector;
	float BurrowElapsed = 0.f;
	float BurrowDuration = 1.f;
	float EmergeElapsed = 0.f;
	bool bEmergeHitDone = false;

	int32 LastPattern = INDEX_NONE;
	TArray<float> PatternReadyTime;
	float NextPatternAllowedTime = 0.f;
	TSet<TWeakObjectPtr<AActor>> HitThisStrike;

	FVector HomeLocation = FVector::ZeroVector;
	FRotator HomeRotation = FRotator::ZeroRotator;   // 도착 시 이 방향으로 선다
	float FightStartTime = 0.f;

	TArray<TWeakObjectPtr<ATDEnemyBase>> Minions;
	FTDStatSourceHandle BossBuffHandle;

	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> TelegraphVFXComponent;

	FTimerHandle PhaseTimerHandle;
	FTimerHandle StrikeTickHandle;
	FTimerHandle EntranceTimerHandle;
	FTimerHandle EnrageTimerHandle;
	FTimerHandle TransitionTimerHandle;
	FTimerHandle ReturnTimeoutHandle;
	FDelegateHandle HealthChangedHandle;
};