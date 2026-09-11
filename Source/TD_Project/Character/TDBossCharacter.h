#pragma once

#include "CoreMinimal.h"
#include "Character/TDEnemyBase.h"
#include "Combat/TDBossPattern.h"
#include "TDBossCharacter.generated.h"

class UCameraShakeBase;
class ATDBossProjectile;
struct FOnAttributeChangeData;

/** 보스 사건 방송. 전 머신. BP 가 포효·UI 를 붙인다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDOnBossEvent, ETDBossEvent, Event, int32, Param);
/** 패턴 예고. 전 머신. Center/Direction 으로 바닥 표시(데칼·VFX)를 그린다. Duration 은 선딜 길이. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FTDOnBossPatternTelegraph,
	int32, PatternIndex, FVector, Center, FVector, Direction, float, Duration);
/** 페이즈 변경. 전 머신(복제). 보스 체력바가 색을 바꾸는 지점. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOnBossPhaseChanged, int32, NewPhase);
/** 패턴 종료. 서버 전용(네이티브). BT 실행 태스크가 기다리는 신호. */
DECLARE_MULTICAST_DELEGATE_OneParam(FTDOnBossPatternFinished, int32 /*PatternIndex*/);

/**
 * 보스 몬스터. 패턴 엔진(선딜→타격→후딜)·페이즈·분노·입장·소환·리시·화면 흔들림.
 *
 * 두뇌(BT)는 "무엇을 할지"만 정하고 StartPattern 을 부른다. 이동(돌진·잠수·투사체)은
 * OnTelegraphBegin/OnMotionBegin/OnMotionEnd 훅에서 이 클래스가 처리한다.
 * 데미지는 전부 CombatStatics::ApplyDamage, 히트 방송은 CombatComponent::NotifyHit —
 * 평타·스킬과 같은 접점을 쓴다.
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
	void ResetFight();
	bool StartPattern(int32 PatternIndex);
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
	int32 GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	bool IsEnraged() const { return bEnraged; }

	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	int32 GetPatternCount() const { return Patterns.Num(); }

	/** BP 가 예고 모양(박스/구·크기)을 그릴 때 읽는다. */
	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	bool GetPatternSpec(int32 PatternIndex, FTDBossPatternSpec& OutSpec) const;

	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	ETDBossPatternPhase GetCurrentPatternPhase() const { return CurrentPatternPhase; }

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

	/** 선딜 시작. 잠수는 여기서 가라앉는다. */
	virtual void OnTelegraphBegin(const FTDBossPatternSpec& Spec);
	/** 타격 시작. 돌진은 달리기 시작, 잠수는 출현, 물대포는 발사. */
	virtual void OnMotionBegin(const FTDBossPatternSpec& Spec);
	/** 타격 끝. 돌진 정지. */
	virtual void OnMotionEnd(const FTDBossPatternSpec& Spec);
	/** 판정 중심. 기본은 정면 오프셋, 잠수는 찍어둔 대상 위치. */
	virtual FVector GetStrikeCenter(const FTDBossPatternSpec& Spec) const;

	// ── 패턴 명세 ────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns")
	TArray<FTDBossPatternSpec> Patterns;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns")
	bool bAvoidRepeatingPattern = true;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns")
	bool bDrawDebugHits = true;

	// ── 이동 파라미터 ─────────────────────────────────────

	/** 돌진 속도(cm/s). 이동 거리 = 속도 × StrikeTime. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float DashSpeed = 1400.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion")
	TSubclassOf<ATDBossProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float ProjectileSpeed = 1200.f;

	/** 발사 지점: 캡슐 반지름 + 이 값만큼 정면. 몸 안에서 스폰되면 벽 판정에 걸린다. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float ProjectileMuzzleForward = 120.f;

	/** 발사 높이(캡슐 중심 기준 Z). 입 위치에 맞춘다. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion")
	float ProjectileMuzzleHeight = 0.f;

	/** 페이즈 2 부채꼴 발사 수. 1 이면 페이즈 2 에도 한 발. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "1"))
	int32 ProjectileCountPhase2 = 3;

	/** 부채꼴 발 사이 각도. */
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

	// ── 분노 ─────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Enrage", meta = (ClampMin = "0"))
	float EnrageAfterSeconds = 240.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Enrage", meta = (ClampMin = "0.1", ClampMax = "1"))
	float EnrageTelegraphScale = 0.5f;

	// ── 전투 ─────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "0"))
	float EntranceDuration = 2.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "1"))
	float RecoveryIncomingDamageMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "0"))
	float LeashRadius = 2500.f;

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

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCameraShake(FVector Epicenter, float Scale);

	UFUNCTION()
	void OnRep_Phase();

	// ── 패턴 진행 (서버) ──────────────────────────────────

	void EnterStrike();
	void DoStrikeHit();
	void EnterRecovery();
	void FinishPattern();
	void CancelPattern();
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

	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void EndEntrance();
	void EndPhase2Transition();
	void DestroyMinions();
	void ClearFightTimers();
	void PlayShakeLocally(const FVector& Epicenter, float Scale) const;

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	int32 Phase = 1;

	UPROPERTY(Replicated)
	bool bEnraged = false;

	/** 패턴 시작 순간의 대상 위치. 잠수가 "그 자리"를 노리게 한다(추적 안 함 → 피할 수 있음). */
	FVector PatternTargetLocation = FVector::ZeroVector;

	TWeakObjectPtr<ATDCharacterBase> PatternTarget;

	ETDBossPatternPhase CurrentPatternPhase = ETDBossPatternPhase::None;
	int32 CurrentPattern = INDEX_NONE;

private:
	bool bFightActive = false;
	bool bInEntrance = false;
	bool bInPhaseTransition = false;

	// 돌진
	bool bDashing = false;
	FVector DashDirection = FVector::ForwardVector;

	// 잠수
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

	FTimerHandle PhaseTimerHandle;
	FTimerHandle StrikeTickHandle;
	FTimerHandle EntranceTimerHandle;
	FTimerHandle EnrageTimerHandle;
	FTimerHandle TransitionTimerHandle;

	FDelegateHandle HealthChangedHandle;
};