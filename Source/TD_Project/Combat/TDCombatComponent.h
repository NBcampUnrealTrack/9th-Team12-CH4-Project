#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TDCombatComponent.generated.h"

/** 피격 발생. 데미지 텍스트(김선우)·히트 VFX(김희진)가 구독하는 공식 접점. 전 머신에서 불린다.
 *  HitLocation: 공격자 쪽에서 대상 콜리전에 닿는 지점. 몸 중심이 필요하면 Target 에서 구한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FTDOnHit, AActor*, Target, float, Damage, bool,
	bCritical, FVector, HitLocation);
/** 공격 모션 시작. 애니메이션·사운드가 구독한다. 전 머신에서 불린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnAttackStarted);
/**
 * 공격 모션 시작 + 어느 공격인지. 몽타주를 고르는 BP 가 구독한다.
 * 번호는 서버가 골라 방송에 실어 보내므로 전 머신이 같은 모션을 본다.
 * OnAttackStarted 와 같은 순간에 불린다 — 번호가 필요 없는 구독자는 그쪽을 쓰면 된다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOnPatternStarted, int32, AttackIndex);

/**
 * 공격 한 종류의 정의. 몽타주 하나 = 스펙 하나. 배열에서의 순서가 곧 AttackIndex 다.
 * 보스 패턴도 이 구조체를 확장해서 쓴다.
 */
USTRUCT(BlueprintType)
struct FTDAttackSpec
{
	GENERATED_BODY()

	/** 공격 시작부터 판정까지(초). 몽타주에서 "휘두르는 순간"을 재서 넣는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float HitDelay = 0.4f;

	/** 기본 쿨타임(초). 쿨다운회복률로 나눠진다. 몽타주 길이보다 길어야 모션이 안 끊긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1"))
	float Cooldown = 0.8f;

	/** 히트박스 절반 크기. X 가 진행 방향. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector HitBoxExtent = FVector(60.f, 80.f, 80.f);

	/** 캐릭터 중심에서 히트박스 중심까지의 전방 거리. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float HitBoxForwardOffset = 100.f;
};

/**
 * 공격 행위를 담당하는 컴포넌트. 쿨타임·히트박스 판정·데미지 호출.
 *
 * Pawn(캐릭터)에 붙는다. 스탯이 PlayerState 에 있는 것과 반대다 —
 * 쿨타임과 히트박스는 아바타의 것이고, 죽으면 리셋되는 게 맞다.
 *
 * 판정과 데미지는 전부 서버. 클라이언트는 ServerRequestAttack 으로 요청만 보낸다.
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDCombatComponent();

	/** 공격 요청. 입력 바인딩과 AI 가 부른다. 어느 공격을 쓸지는 서버가 고른다. 검증 실패는 조용히 무시된다. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Combat")
	void ServerRequestAttack();

	/** 특정 공격을 지정해 요청한다. 보스 패턴처럼 "무엇을 쓸지"를 밖에서 정할 때. 서버 전용. */
	void RequestAttack(int32 AttackIndex);

	/**
	 * 피격 방송. 이 컴포넌트 밖에서 판정한 타격(스킬)도 같은 델리게이트로 흘려보낸다.
	 *
	 * 스킬이 자기 OnHit 을 따로 갖지 않게 하려는 것이다. 델리게이트가 둘이면
	 * 데미지 텍스트와 히트 VFX 가 양쪽을 다 구독해야 하고, 한쪽을 빠뜨리면
	 * "평타는 숫자가 뜨는데 스킬은 안 뜨는" 상태가 된다.
	 *
	 * 서버에서만 의미가 있다. 클라이언트 호출은 조용히 무시된다.
	 */
	void NotifyHit(AActor* Target, float Damage, bool bCritical, FVector HitLocation);

	UPROPERTY(BlueprintAssignable, Category = "TD|Combat")
	FTDOnHit OnHit;

	UPROPERTY(BlueprintAssignable, Category = "TD|Combat")
	FTDOnAttackStarted OnAttackStarted;

	UPROPERTY(BlueprintAssignable, Category = "TD|Combat")
	FTDOnPatternStarted OnPatternStarted;

	/** 히트박스가 향할 방향(XY 단위벡터). 마지막 이동 방향이며, AI 는 공격 직전 명시한다. */
	UFUNCTION(BlueprintPure, Category = "TD|Combat")
	FVector GetFacingDirection() const { return FacingDirection; }

	/** 방향을 직접 지정한다. 0 벡터(제자리)는 무시 — 마지막 방향이 유지된다. */
	void SetFacingDirection(const FVector& Direction);

	/** 예약된 타격 판정을 취소한다. 경직·사망이 스윙을 끊을 때 쓴다. 서버 전용. */
	void CancelAttack();

	UFUNCTION(BlueprintPure, Category = "TD|Combat")
	int32 GetAttackCount() const { return Attacks.Num(); }

protected:
	/**
	 * 이 캐릭터가 가진 공격들. 비어 있으면 공격하지 않는다.
	 * 기본은 평타 1종. 몬스터 BP 에서 몽타주 수만큼 늘리고 각각의 HitDelay 를 잰 값으로 채운다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Combat")
	TArray<FTDAttackSpec> Attacks;

	/** 켜면 직전과 같은 공격을 연속으로 고르지 않는다. 2종이면 사실상 번갈아 나온다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Combat")
	bool bAvoidRepeatingAttack = true;

	/** 켜면 공격할 때 히트박스를 그려준다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Combat")
	bool bDrawDebugHitBox = true;

	virtual void BeginPlay() override;

	/** 이동이 갱신될 때마다 불린다. 속도가 있으면 그 방향을 기억한다 — ABP 의 SetDirectionality 와 같은 규칙. */
	UFUNCTION()
	void HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	/** 공격 시작 방송. 각 머신에서 OnAttackStarted 와 OnPatternStarted 를 발화시킨다. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastOnAttack(int32 AttackIndex);

	/** 지연이 끝난 뒤 실제 판정. 서버 전용. */
	void PerformHit();

	/** "맞았다" 방송. 서버가 판정하고 전원이 OnHit 델리게이트로 전달받는다. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastOnHit(AActor* Target, float Damage, bool bCritical, FVector HitLocation);

	bool CanAttack() const;

	/** 다음 공격 번호. 랜덤이며 bAvoidRepeatingAttack 이면 직전 것을 피한다. */
	int32 ChooseAttackIndex() const;

	/** 스펙의 박스 안에서 유효 대상(적팀·생존자)을 모은다. 서버 전용. */
	TArray<AActor*> GatherTargets(const FTDAttackSpec& Spec) const;

private:
	/** 마지막 공격 시각(서버 월드시간). 복제하지 않는다 — 판정은 서버만 하므로. */
	float LastAttackTime = -1000.f;

	/** 마지막 공격의 실제 쿨타임(회복률 적용 후). 공격마다 다를 수 있어 시전 때 계산해 둔다. */
	float CurrentCooldown = 0.f;

	int32 LastAttackIndex = INDEX_NONE;

	/** 타이머가 판정할 공격. 타이머가 울릴 때 어느 스펙의 박스를 쓸지 알아야 한다. */
	int32 PendingAttackIndex = INDEX_NONE;

	FTimerHandle HitTimerHandle;

	/** 마지막 이동 방향. 서 있어도 유지된다. BeginPlay 에서 액터 정면으로 초기화. */
	FVector FacingDirection = FVector::ForwardVector;
};