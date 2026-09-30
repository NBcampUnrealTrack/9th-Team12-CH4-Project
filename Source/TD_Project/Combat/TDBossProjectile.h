#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDBossProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class ATDCharacterBase;
class UNiagaraSystem;

/**
 * 보스 원거리 패턴(물대포)의 탄. 판정만 있고 그림은 BP 자식이 붙인다.
 * 서버가 스폰·판정하고 이동은 복제로 보인다. 맞으면 ApplyDamage → 보스 CombatComponent 의
 * NotifyHit 으로 흘려 평타·스킬과 같은 OnHit 접점(팝업·VFX)을 탄다.
 */
UCLASS()
class TD_PROJECT_API ATDBossProjectile : public AActor
{
	GENERATED_BODY()

public:
	ATDBossProjectile();

	/** 스폰 직후 서버가 한 번 부른다. 방향·속도·배율을 확정한다. */
	void Init(ATDCharacterBase* InShooter, const FVector& Direction, float InDamageScale, float Speed, float InKnockback);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleStop(const FHitResult& ImpactResult);

	UPROPERTY(VisibleAnywhere, Category = "TD|Projectile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "TD|Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	/** 아무것도 안 맞으면 이 시간 뒤 소멸. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Projectile", meta = (ClampMin = "0.1"))
	float LifeSeconds = 3.f;

	/** 켜면 맞아도 안 사라지고 관통한다(한 대상 1회). */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Projectile")
	bool bPierce = false;

	/** 명중·벽 충돌 순간 그 자리에 한 번 터지는 이펙트. 서버가 모두에게 방송한다. 날아가는 그림(궤적)은 BP 에 Niagara 컴포넌트를 붙이면 된다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Projectile|VFX")
	TSoftObjectPtr<UNiagaraSystem> ImpactVFX;

	UPROPERTY(EditDefaultsOnly, Category = "TD|Projectile|VFX", meta = (ClampMin = "0.01"))
	float ImpactVFXScale = 1.f;

	/** 명중·벽 충돌 순간 모든 기기에서 한 번. 소리·데칼 등 BP 추가 연출용. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TD|Projectile|VFX")
	void OnImpact(FVector Location);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastImpact(FVector Location);

private:
	TWeakObjectPtr<ATDCharacterBase> Shooter;
	float DamageScale = 1.f;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	
	float Knockback = 0.f;
};