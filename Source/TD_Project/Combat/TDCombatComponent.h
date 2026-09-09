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

	/** 공격 요청. 입력 바인딩과 AI 가 부른다. 검증 실패는 조용히 무시된다(치터 대응). */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Combat")
	void ServerRequestAttack();

	UPROPERTY(BlueprintAssignable, Category = "TD|Combat")
	FTDOnHit OnHit;

	UPROPERTY(BlueprintAssignable, Category = "TD|Combat")
	FTDOnAttackStarted OnAttackStarted;
	
	/** 히트박스가 향할 방향(XY 단위벡터). 마지막 이동 방향이며, AI 는 공격 직전 명시한다. */
	UFUNCTION(BlueprintPure, Category = "TD|Combat")
	FVector GetFacingDirection() const { return FacingDirection; }

	/** 방향을 직접 지정한다. 0 벡터(제자리)는 무시 — 마지막 방향이 유지된다. */
	void SetFacingDirection(const FVector& Direction);

	/** 예약된 타격 판정을 취소한다. 경직이 스윙을 끊을 때 쓴다. 서버 전용. */
	void CancelAttack();
	
protected:
	/** 기본 쿨타임(초). 쿨다운회복률 스탯으로 나눠져 실제 쿨타임이 된다. 추후 직업·스킬 테이블로 이관. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Combat", meta = (ClampMin = "0.1"))
	float AttackCooldown = 0.8f;

	/** 전방 히트박스 절반 크기. 2D 스프라이트의 타격 범위와 눈으로 맞춰가며 조절한다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Combat")
	FVector HitBoxExtent = FVector(60.f, 80.f, 80.f);

	/** 캐릭터 중심에서 히트박스 중심까지의 전방 거리. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Combat")
	float HitBoxForwardOffset = 100.f;

	/** 켜면 공격할 때 히트박스를 그려준다. 스프라이트가 없는 지금은 사실상 필수. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Combat")
	bool bDrawDebugHitBox = true;

	/** 공격 시작부터 실제 타격 판정까지의 지연(초). 애니메이션의 "휘두르는 순간"에 맞춘다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Combat", meta = (ClampMin = "0"))
	float HitDelay = 0.4f;
	
	/** 공격 시작 방송. 각 머신에서 OnAttackStarted 를 발화시킨다. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastOnAttack();

	/** 지연이 끝난 뒤 실제 판정. 서버 전용. */
	void PerformHit();
	
	/** "맞았다" 방송. 서버가 판정하고 전원이 OnHit 델리게이트로 전달받는다. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastOnHit(AActor* Target, float Damage, bool bCritical, FVector HitLocation);

	bool CanAttack() const;

	/** 전방 박스 안의 유효 대상(적팀·생존자)을 모은다. 서버 전용. */
	TArray<AActor*> GatherTargets() const;
	
	virtual void BeginPlay() override;

	/** 이동이 갱신될 때마다 불린다. 속도가 있으면 그 방향을 기억한다 — ABP 의 SetDirectionality 와 같은 규칙. */
	UFUNCTION()
	void HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

private:
	/** 마지막 공격 시각(서버 월드시간). 복제하지 않는다 — 판정은 서버만 하므로. */
	float LastAttackTime = -1000.f;
	
	FTimerHandle HitTimerHandle;
	
	/** 마지막 이동 방향. 서 있어도 유지된다. BeginPlay 에서 액터 정면으로 초기화. */
	FVector FacingDirection = FVector::ForwardVector;
};