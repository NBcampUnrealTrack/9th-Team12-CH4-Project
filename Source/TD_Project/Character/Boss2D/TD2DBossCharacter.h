#pragma once

#include "CoreMinimal.h"
#include "Character/TDBossCharacter.h"
#include "Character/Boss2D/TD2DFlipbookSet.h"
#include "TD2DBossCharacter.generated.h"

class UNiagaraSystem;
class UPaperFlipbook;
class USoundBase;

/** Boss2D BP가 점프·착지 임팩트 연출을 고르는 공식 신호. */
UENUM(BlueprintType)
enum class ETD2DBossAction : uint8
{
	JumpStarted,
	Landed,
	Phase3Started,
	/** FlameLine 본 타격 순간. 기본공격 사운드 재생에 사용한다. */
	BasicAttack
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOn2DBossAction, ETD2DBossAction, Action);

/**
 * 원형 경기장 안에서 싸우는 PaperZD 보스.
 *
 * 기존 ATDBossCharacter의 스탯·보상·패턴·보스 UI를 그대로 쓰고 다음만 더한다.
 * - 고정 HomeLocation 중심의 경기장 감지/이동 제한
 * - JumpSlam 패턴의 포물선 이동
 * - 착지 순간 1회 피해를 주는 내려찍기 임팩트
 */
UCLASS()
class TD_PROJECT_API ATD2DBossCharacter : public ATDBossCharacter
{
	GENERATED_BODY()

public:
	ATD2DBossCharacter();

	virtual void Tick(float DeltaSeconds) override;
	/** 전용 AI의 리셋 진입점. 부모의 비가상 API는 그대로 둔다. */
	void ResetArenaFight(bool bInstant = false);
	bool IsArenaBusy() const { return bPhase3Transition || IsBusy(); }
	virtual bool IsInvulnerable() const override;

	UFUNCTION(BlueprintPure, Category = "TD|2D Boss|Arena")
	float GetArenaRadius() const { return ArenaRadius; }

	UFUNCTION(BlueprintPure, Category = "TD|2D Boss|Arena")
	FVector GetArenaCenter() const { return GetHomeLocation(); }

	UFUNCTION(BlueprintPure, Category = "TD|2D Boss|Arena")
	bool IsLocationInsideArena(const FVector& Location) const;

	/** 캡슐/연출 여백까지 빼고 목적지를 원 안으로 보정한다. Z는 입력값을 유지한다. */
	UFUNCTION(BlueprintPure, Category = "TD|2D Boss|Arena")
	FVector ClampLocationToArena(const FVector& Location, float ExtraMargin = 0.f) const;

	/** 고정 경기장 안의 가장 가까운 적. 움직이는 보스 위치가 감지 원의 중심이 되지 않는다. */
	ATDCharacterBase* FindNearestEnemyInArena() const;

	UPROPERTY(BlueprintAssignable, Category = "TD|2D Boss|Animation")
	FTDOn2DBossAction On2DBossAction;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleDeath() override;
	virtual void OnTelegraphBegin(const FTDBossPatternSpec& Spec) override;
	virtual void OnMotionBegin(const FTDBossPatternSpec& Spec) override;
	virtual void OnMotionEnd(const FTDBossPatternSpec& Spec) override;
	virtual FVector GetStrikeBase(const FTDBossPatternSpec& Spec) const override;

	// 공통 보스가 아닌 이 보스에만 필요한 3페이즈 / 반복 소환 설정.
	UPROPERTY(EditAnywhere, Category = "TD|2D Boss|Phase", meta = (ClampMin = "0", ClampMax = "1"))
	float Phase3HealthRatio = 0.4f;
	UPROPERTY(EditAnywhere, Category = "TD|2D Boss|Phase", meta = (ClampMin = "0"))
	float Phase3TransitionDuration = 1.f;
	UPROPERTY(EditAnywhere, Category = "TD|2D Boss|Summon")
	bool bRepeatSummonFromPhase2 = true;
	UPROPERTY(EditAnywhere, Category = "TD|2D Boss|Summon", meta = (ClampMin = "0.1"))
	float SummonInterval = 2.f;
	UPROPERTY(EditAnywhere, Category = "TD|2D Boss|Summon", meta = (ClampMin = "1"))
	int32 MaxActiveMinions = 10;

	/** 기존 Motion enum을 확장하지 않고 이 이름의 패턴만 점프 모션을 사용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|2D Boss|Jump Slam")
	FName JumpSlamPatternName = TEXT("JumpSlam");

	/** 이 이름의 패턴이 1~2페이즈 기본공격 사운드를 재생한다. 기본값은 FlameLine. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|2D Boss|Audio")
	FName BasicAttackPatternName = TEXT("FlameLine");

	// ── 경기장 ────────────────────────────────────────────
	/** HomeLocation 중심의 XY 반경. BP 클래스 디폴트나 배치 인스턴스에서 조절한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|2D Boss|Arena", meta = (ClampMin = "100"))
	float ArenaRadius = 1500.f;

	/** 보스 캡슐이 경계에 걸리지 않도록 반경에서 추가로 빼는 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|2D Boss|Arena", meta = (ClampMin = "0"))
	float ArenaBoundaryMargin = 25.f;

	// ── 내려찍기 ──────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|2D Boss|Jump Slam", meta = (ClampMin = "0"))
	float JumpHeight = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|2D Boss|Jump Slam")
	TObjectPtr<UNiagaraSystem> LandingVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|2D Boss|Jump Slam")
	TObjectPtr<USoundBase> LandingSound;

	/** 기본공격(FlameLine) 본 타격 순간 클라이언트에서 1회 재생할 사운드. 비워두면 무음. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|2D Boss|Audio")
	TObjectPtr<USoundBase> BasicAttackSound;

	// ── 직접 PaperFlipbook 애니메이션 ───────────────────
	// ABP_Dullahan이 비어 있어도 여섯 동작과 4방향 플립북을 모두 재생한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|2D Boss|Animation")
	FTD2DDirectionalFlipbooks IdleAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|2D Boss|Animation")
	FTD2DDirectionalFlipbooks WalkAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|2D Boss|Animation")
	FTD2DDirectionalFlipbooks JumpAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|2D Boss|Animation")
	FTD2DDirectionalFlipbooks AttackAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|2D Boss|Animation")
	FTD2DDirectionalFlipbooks HitAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|2D Boss|Animation")
	FTD2DDirectionalFlipbooks DieAnimation;

	/** Die 플립북 재생이 끝난 뒤 마지막 프레임을 유지하는 초. 이 보스는 CorpseLifetime 대신 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|2D Boss|Animation", meta = (ClampMin = "0", Units = "s"))
	float DeathHoldDuration = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|2D Boss|Animation", meta = (ClampMin = "0"))
	float HitAnimationDuration = 0.25f;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast2DBossAction(ETD2DBossAction Action, FVector Location);

private:
	enum class EVisualAction : uint8
	{
		None,
		Attack,
		Jump
	};

	void StartJumpSlam(float Duration);
	void TickJumpSlam(float DeltaSeconds);
	void FinishJumpSlam();
	void AbortJumpSlam();

	void PlayLandingVFX(FVector Location);
	void HandleArenaHealthChanged(const FOnAttributeChangeData& Data);
	void EnterArenaPhase3();
	void FinishArenaPhase3Transition();
	void StartArenaSummons();
	void SummonArenaMinions();
	void ClearArenaFight();
	void HandleArenaPatternFinished(int32 PatternIndex);
	bool IsJumpSlamPattern(const FTDBossPatternSpec& Spec) const { return Spec.Name == JumpSlamPatternName; }
	void RefreshFlipbook();
	UPaperFlipbook* PickDirectionalFlipbook(const FTD2DDirectionalFlipbooks& Set) const;

	UFUNCTION()
	void HandleBossVisualEvent(ETDBossEvent Event, int32 Param);

	UFUNCTION()
	void HandlePatternVisual(int32 PatternIndex, FVector Center, FVector Direction, float Duration);

	UFUNCTION()
	void HandleDamagedVisual(AActor* Attacker, float Damage, bool bCritical);

	UFUNCTION()
	void HandleRespawnVisual();

	bool bJumping = false;
	FVector JumpFrom = FVector::ZeroVector;
	FVector JumpTo = FVector::ZeroVector;
	float JumpElapsed = 0.f;
	float JumpDuration = 1.f;

	FTimerHandle ArenaSummonTimerHandle;
	FTimerHandle ArenaPhase3TimerHandle;
	FDelegateHandle ArenaHealthChangedHandle;
	FDelegateHandle ArenaPatternFinishedHandle;
	bool bPhase3Transition = false;
	TArray<TWeakObjectPtr<ATDEnemyBase>> ArenaMinions;

	EVisualAction VisualAction = EVisualAction::None;
	bool bVisualDead = false;
	FVector VisualFacing = FVector::ForwardVector;
	float HitVisualEndTime = -1.f;

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> CurrentVisualFlipbook;
};
