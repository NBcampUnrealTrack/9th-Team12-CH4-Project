#pragma once

#include "CoreMinimal.h"
#include "Character/TDEnemyBase.h"
#include "Combat/TDBossPattern.h"
#include "TDBossCharacter.generated.h"

class UCameraShakeBase;
struct FOnAttributeChangeData;

/** 보스 사건 방송. 전 머신. BP 가 예고 VFX·포효·UI 를 붙인다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDOnBossEvent, ETDBossEvent, Event, int32, Param);
/** 페이즈 변경. 전 머신(복제). 보스 체력바가 색을 바꾸는 지점. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOnBossPhaseChanged, int32, NewPhase);
/** 패턴 종료. 서버 전용(네이티브). BT 실행 태스크가 기다리는 신호. */
DECLARE_MULTICAST_DELEGATE_OneParam(FTDOnBossPatternFinished, int32 /*PatternIndex*/);

/**
 * 보스 몬스터. 패턴 엔진(선딜→타격→후딜)·페이즈·분노·입장·소환·리시·화면 흔들림을 가진다.
 *
 * 두뇌(BT)는 "무엇을 할지"만 정하고 StartPattern 을 부른다. 이동(돌진·잠수·투사체)은
 * OnMotionBegin/End 훅으로 자식 또는 이 클래스의 2단계 구현이 맡는다.
 * 데미지는 전부 CombatStatics::ApplyDamage — 새 경로를 만들지 않는다.
 */
UCLASS()
class TD_PROJECT_API ATDBossCharacter : public ATDEnemyBase
{
	GENERATED_BODY()

public:
	ATDBossCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── 서버 API: BT·치트가 부른다 ────────────────────────

	/** 전투 시작. 입장 연출(무적·정지) 후 패턴이 가능해진다. 분노 타이머도 여기서 시작. */
	void BeginFight(ATDCharacterBase* FirstTarget);

	/** 리시 귀환: 집으로 돌아가 풀피·페이즈 1·쫄 정리. */
	void ResetFight();

	/** 패턴 실행. 바쁘거나(진행 중·입장·전환) 죽었으면 false. */
	bool StartPattern(int32 PatternIndex);

	/** 거리·쿨·가중치·직전 패턴을 보고 다음 패턴을 고른다. 준비된 게 없으면 INDEX_NONE. */
	int32 ChoosePattern(float DistanceToTarget) const;

	void SetPatternTarget(ATDCharacterBase* Target);

	/** 적 탐색. bFarthest 면 가장 먼 적(잠수 패턴·원거리 견제용), 아니면 가장 가까운 적. */
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

	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	ETDBossPatternPhase GetCurrentPatternPhase() const { return CurrentPatternPhase; }

	FVector GetHomeLocation() const { return HomeLocation; }
	float GetLeashRadius() const { return LeashRadius; }
	ATDCharacterBase* GetPatternTarget() const { return PatternTarget.Get(); }

	// ── 델리게이트 ────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "TD|Boss")
	FTDOnBossEvent OnBossEvent;

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

	// ── 이동 훅: 2단계에서 채운다 ──────────────────────────

	/** 타격 구간 시작. 돌진이면 여기서 달리기 시작, 잠수면 여기서 솟구침. */
	virtual void OnMotionBegin(const FTDBossPatternSpec& Spec) {}
	/** 타격 구간 끝. 돌진 정지 등. */
	virtual void OnMotionEnd(const FTDBossPatternSpec& Spec) {}
	/** 판정 중심. 기본은 정면 오프셋. 잠수는 PatternTargetLocation 으로 바꾼다. */
	virtual FVector GetStrikeCenter(const FTDBossPatternSpec& Spec) const;

	// ── 패턴 명세 ────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns")
	TArray<FTDBossPatternSpec> Patterns;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns")
	bool bAvoidRepeatingPattern = true;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Patterns")
	bool bDrawDebugHits = true;

	// ── 페이즈 ───────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Phase", meta = (ClampMin = "0", ClampMax = "1"))
	float Phase2HealthRatio = 0.5f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Phase", meta = (ClampMin = "0.1", ClampMax = "1"))
	float Phase2TelegraphScale = 0.7f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Phase", meta = (ClampMin = "0.1", ClampMax = "1"))
	float Phase2RecoveryScale = 0.8f;

	/** 전환 연출 시간. 이 동안 무적·정지. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Phase", meta = (ClampMin = "0"))
	float Phase2TransitionDuration = 2.f;

	// ── 분노 ─────────────────────────────────────────────

	/** 전투 시작 후 이 시간이 지나면 분노. 0 이면 없음. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Enrage", meta = (ClampMin = "0"))
	float EnrageAfterSeconds = 240.f;

	UPROPERTY(EditAnywhere, Category = "TD|Boss|Enrage", meta = (ClampMin = "0.1", ClampMax = "1"))
	float EnrageTelegraphScale = 0.5f;

	// ── 전투 ─────────────────────────────────────────────

	/** 입장 연출 시간. 무적·정지. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "0"))
	float EntranceDuration = 2.f;

	/** 후딜(빈틈) 동안 받는 피해 배율. "때릴 타이밍"을 만든다. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Fight", meta = (ClampMin = "1"))
	float RecoveryIncomingDamageMultiplier = 1.5f;

	/** 집에서 이 거리보다 멀어지면 리시(귀환). BT 가 검사한다. */
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

	/** 이 거리 안은 전체 강도. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Feedback", meta = (ClampMin = "0"))
	float ShakeInnerRadius = 600.f;

	/** 이 거리 밖은 안 흔들림. 사이는 선형 감쇠. */
	UPROPERTY(EditAnywhere, Category = "TD|Boss|Feedback", meta = (ClampMin = "0"))
	float ShakeOuterRadius = 2500.f;

	// ── 방송 ─────────────────────────────────────────────

	UFUNCTION(NetMulticast, Reliable)
	void MulticastBossEvent(ETDBossEvent Event, int32 Param);

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