#pragma once

#include "CoreMinimal.h"
#include "Character/TDEnemyBase.h"
#include "Combat/TDBossPattern.h"
#include "Stats/TDStatTypes.h"
#include "TDBossCharacter.generated.h"

class UCameraShakeBase;
class UNiagaraComponent;
class UNiagaraSystem;
class ATDBossProjectile;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDOnBossEvent, ETDBossEvent, Event, int32, Param);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FTDOnBossPatternTelegraph,
	int32, PatternIndex, FVector, Center, FVector, Direction, float, Duration);
/** 추가 타격 예고. Center 는 그 판정의 중심(발 높이), Duration 뒤에 떨어진다. 바닥 인디케이터를 붙이는 지점. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FTDOnBossExtraTelegraph,
	int32, PatternIndex, int32, ExtraIndex, FVector, Center, FVector, Direction, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOnBossPhaseChanged, int32, NewPhase);
DECLARE_MULTICAST_DELEGATE_OneParam(FTDOnBossPatternFinished, int32 /*PatternIndex*/);

/** 추가 타격 하나가 띄워 둔 예고 VFX 묶음. 머신마다 자기 것. */
USTRUCT()
struct FTDBossVFXBatch
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> Components;
};

/**
 * 보스 몬스터. 패턴 엔진(선딜→타격→후딜)·페이즈·분노·입장·소환·리시 귀환·흔들림·VFX·몸 연출.
 * 두뇌(BT)가 "무엇을"을 정하면 이 클래스가 "어떻게"를 한다. 이동·회전·솟구침은 서버 Tick.
 *
 * 판정 높이는 전부 발밑(GetFloorZ) 기준이다. 원판·도넛은 바닥 기준 원기둥으로 재서 예고 원과 정확히 같다.
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
	/** 범위 안의 살아 있는 적 중 무작위 하나. Exclude 는 다른 후보가 있을 때만 제외한다(혼자면 그대로). */
	ATDCharacterBase* FindRandomEnemy(float MaxRange, const ATDCharacterBase* Exclude = nullptr) const;
	/**
	 * 어그로 교체 시점인가. RetargetInterval 마다 한 번 true 를 돌려주고 다음 시점을 예약한다.
	 * 부르는 쪽(BossSense)이 "패턴 중이 아닐 때"만 물어보므로, 교체는 패턴 사이에서만 일어난다.
	 */
	bool ShouldRetargetNow();
	void EnterPhase2();
	void TriggerEnrage();
	void SummonMinions();
	/** 지금 거리·페이즈에서 쓸 수 있는 패턴이 하나라도 있나. 쉼·쿨·거리 조건을 본다(랜덤은 안 굴림). */
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
	UPROPERTY(BlueprintAssignable, Category = "TD|Boss") FTDOnBossExtraTelegraph OnExtraTelegraph;
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

	/** 본 판정의 기준점(발 높이). 잠수는 대상 자리, 나머지는 보스 자리. 여기에 ForwardOffset 이 더해진다. */
	virtual FVector GetStrikeBase(const FTDBossPatternSpec& Spec) const;
	/** 발밑 Z. 솟구침 중엔 액터가 공중이라 잠수 목적지(지면 캡슐 중심) 기준. */
	float GetFloorZ() const;

	// ── 패턴 명세 ────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns") TArray<FTDBossPatternSpec> Patterns;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns") bool bAvoidRepeatingPattern = true;
	/** 서버 창에 예고(노랑)·타격(빨강) 판정을 그린다. 예고 그림과 맞는 범위가 같은지 볼 때. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns") bool bDrawDebugHits = true;

	/** 패턴과 패턴 사이 쉬는 시간(초) 범위. 이 동안 걷고 재배치한다 — 없으면 쉴 새 없이 두들긴다. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns", meta = (ClampMin = "0"))
	float PatternGapMin = 1.0f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns", meta = (ClampMin = "0"))
	float PatternGapMax = 2.0f;

	/** 모든 선딜(잠수 출현 예고 포함)에 곱하는 전역 배율. 0.8 = 전체 20% 빠르게. 페이즈 2·분노 배율은 이 위에 또 곱해진다. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns", meta = (ClampMin = "0.1", ClampMax = "2"))
	float TelegraphScale = 1.0f;

	// ── 몸 연출 ──────────────────────────────────────────
	/** 선딜 동안 몸이 판정 방향으로 도는 속도(도/초). */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float TurnSpeedDegrees = 540.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float DashSpeed = 1400.f;

	/** 돌진 중 몸에 닿은 캐릭터를 밀어내는 속도. 겹치지 않고 튕겨 나간다. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float DashShove = 900.f;

	/** 잠수 시작: 이 시간(초) 동안 제자리에서 땅속(EmergeDepth)으로 가라앉은 뒤 사라진다. 0 이면 바로 사라짐. 선딜(TelegraphTime)에 포함된다. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float DiveTime = 0.4f;

	/** 가라앉는 동안 머리를 이만큼 아래로 기울인다(도). 0 이면 수평 그대로. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0", ClampMax = "80"))
	float DiveTiltDegrees = 30.f;

	/** 가라앉기 시작 순간 발밑에 한 번 터지는 이펙트(물보라). 모든 기기에 방송. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion")
	TSoftObjectPtr<UNiagaraSystem> DiveVFX;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0.01"))
	float DiveVFXScale = 1.f;

	/** 물보라를 이 시간 뒤에 끈다(초). 0 = 이펙트가 알아서 끝남. 무한 루프 이펙트면 반드시 준다. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0"))
	float DiveVFXDuration = 0.f;

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
	/** 페이즈 2 기본 탄 수. 스펙의 ProjectileCount 가 0 이 아니면 그쪽이 우선. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "1")) int32 ProjectileCountPhase2 = 3;
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Motion", meta = (ClampMin = "0")) float ProjectileSpreadAngle = 20.f;

	// ── 페이즈·분노·전투·소환·흔들림 ────────────────────
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
	/** 어그로 무작위 교체 주기(초). 0 이면 교체하지 않고 처음 잡은 대상을 끝까지 문다. 잠수는 이와 별개로 항상 가장 먼 적. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "0")) float RetargetInterval = 20.f;
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
	/** Center 는 본 판정 기준점(발 높이). VFX 는 여기서 스펙대로 갈래·둘레 지점을 계산해 띄운다. */
	UFUNCTION(NetMulticast, Reliable) void MulticastPatternTelegraph(int32 PatternIndex, FVector Center, FVector Direction, float Duration);
	UFUNCTION(NetMulticast, Reliable) void MulticastPatternStrike(int32 PatternIndex, FVector Center, FVector Direction);
	UFUNCTION(NetMulticast, Reliable) void MulticastExtraTelegraph(int32 PatternIndex, int32 ExtraIndex, FVector Center, FVector Direction, float Duration);
	UFUNCTION(NetMulticast, Reliable) void MulticastExtraStrike(int32 PatternIndex, int32 ExtraIndex, FVector Center, FVector Direction);
	/** 취소·사망·리셋: 떠 있는 예고 VFX 를 전 머신에서 지운다. */
	UFUNCTION(NetMulticast, Reliable) void MulticastClearTelegraphs();
	/** 한 번 터지고 스스로 사라지는 이펙트를 모든 기기에서 그 자리에 스폰. 잠수 물보라 등. */
	UFUNCTION(NetMulticast, Reliable) void MulticastOneShotVFX(UNiagaraSystem* System, FVector Location, float Scale, float KillAfter);
	UFUNCTION(NetMulticast, Unreliable) void MulticastCameraShake(FVector Epicenter, float Scale);
	UFUNCTION() void OnRep_Phase();

	// ── 패턴 진행 ────────────────────────────────────────
	void EnterStrike();
	/** 잠수: 땅속 이동이 끝남. 지금 대상 위치를 출현 자리로 잡고 짧은 예고 뒤 EnterStrike. */
	void EndBurrowTravel();
	void DoStrikeHit();
	void EnterRecovery();
	void FinishPattern();
	void StartDashReaim();
	float ScaledTelegraph(float Base) const;
	float ScaledRecovery(float Base) const;
	void ApplyKnockback(ATDCharacterBase* Target, const FVector& Direction, float Strength, float UpRatio) const;

	// ── 판정 영역 (본 판정·추가 타격 공용) ────────────────
	static FTDBossHitArea MakePrimaryArea(const FTDBossPatternSpec& Spec);
	/** 영역 안의 적. Center 는 발 높이 기준점, Facing 은 박스 정면(갈래의 첫 방향). */
	TArray<ATDCharacterBase*> GatherInArea(const FTDBossHitArea& Area, const FVector& Center, const FVector& Facing) const;
	/** 피해 + 히트 방송 + 밀어내기. AlreadyHit 가 있으면 한 대상 1회를 보장한다. */
	void StrikeArea(const FTDBossHitArea& Area, const FVector& Center, const FVector& Facing,
		float DamageScale, float Knockback, float KnockUpRatio, TSet<TWeakObjectPtr<AActor>>* AlreadyHit);
	void DrawArea(const FTDBossHitArea& Area, const FVector& Center, const FVector& Facing, const FColor& Color, float Duration) const;
	/**
	 * 영역 모양대로 VFX 를 띄운다. KeepIn 이 있으면 컴포넌트를 거기 모아 나중에 지운다(예고).
	 * ReferenceSize > 0 이면 판정 크기 / 기준 크기로 자동 배율. Duration 은 User.Duration 으로 넘긴다.
	 * AutoKillAfter > 0 이면 그 시간 뒤 끈다 — 무한 루프 이펙트가 남지 않게.
	 */
	void SpawnAreaVFX(const FTDBossHitArea& Area, const FVector& Center, const FVector& Facing,
		UNiagaraSystem* System, float Scale, float HeightScale, int32 PointCount, int32 RingLayers, float ReferenceSize,
		float HeightOffset, float Duration, float AutoKillAfter, TArray<TObjectPtr<UNiagaraComponent>>* KeepIn);
	void ScheduleExtraStrikes(int32 PatternIndex, const FVector& PrimaryBase, const FVector& Facing);
	void DoExtraStrike(int32 PatternIndex, int32 ExtraIndex, FVector Center, FVector Facing);

	// ── 이동·몸 ──────────────────────────────────────────
	void TickTurn(float DeltaSeconds);
	void StartDash();
	void TickDash(float DeltaSeconds);
	void EndDash(bool bHitWall);
	void StartBurrow(const FTDBossPatternSpec& Spec);
	void TickDive(float DeltaSeconds);
	/** 가라앉기 끝: 숨기고 땅속 이동으로 넘어간다. */
	void FinishDive();
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
	void ClearExtraStrikeTimers();
	void PlayShakeLocally(const FVector& Epicenter, float Scale) const;
	void ClearTelegraphVFX();
	void ClearExtraTelegraphVFX(int32 ExtraIndex);

	UPROPERTY(ReplicatedUsing = OnRep_Phase) int32 Phase = 1;
	UPROPERTY(Replicated) bool bEnraged = false;

	FVector PatternTargetLocation = FVector::ZeroVector;
	TWeakObjectPtr<ATDCharacterBase> PatternTarget;
	ETDBossPatternPhase CurrentPatternPhase = ETDBossPatternPhase::None;
	int32 CurrentPattern = INDEX_NONE;

private:
	/** 원판·도넛 판정의 세로 반경. 바닥에서 이 두 배 높이까지 맞는다. */
	static constexpr float HitCylinderHalfHeight = 250.f;

	bool bFightActive = false;
	bool bInEntrance = false;
	bool bInPhaseTransition = false;
	bool bReturning = false;

	bool bDashing = false;
	FVector DashDirection = FVector::ForwardVector;
	int32 DashesLeft = 1;

	// 잠수: 가라앉기(보임) → 땅속 이동(숨김) → 솟구침(상승·하강) → 착지
	bool bDiving = false;
	float DiveElapsed = 0.f;
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
	bool bExtrasScheduled = false;
	TArray<FTimerHandle> ExtraStrikeHandles;

	FVector HomeLocation = FVector::ZeroVector;
	FRotator HomeRotation = FRotator::ZeroRotator;   // 도착 시 이 방향으로 선다
	float FightStartTime = 0.f;
	float NextRetargetTime = 0.f;   // 다음 어그로 교체 시각(월드 시간)

	TArray<TWeakObjectPtr<ATDEnemyBase>> Minions;
	FTDStatSourceHandle BossBuffHandle;

	/** 본 판정 예고 VFX(박스 갈래·도넛 둘레만큼 여러 개). 머신마다 자기 것. */
	UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> TelegraphVFXComponents;
	/** 추가 타격별 예고 VFX. 인덱스 = ExtraStrikes 인덱스. */
	UPROPERTY(Transient) TArray<FTDBossVFXBatch> ExtraTelegraphVFX;

	FTimerHandle PhaseTimerHandle;
	FTimerHandle StrikeTickHandle;
	FTimerHandle EntranceTimerHandle;
	FTimerHandle EnrageTimerHandle;
	FTimerHandle TransitionTimerHandle;
	FTimerHandle ReturnTimeoutHandle;
	FDelegateHandle HealthChangedHandle;
};
