#pragma once

#include "CoreMinimal.h"
#include "Character/TDEnemyBase.h"
#include "Character/Boss2D/TD2DFlipbookSet.h"
#include "TDExploderMinion.generated.h"

class ATD2DBossCharacter;
class UNiagaraSystem;
class UPaperFlipbook;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnExploderExploded);

/** 플레이어에게 접근하면 즉시 한 번 폭발하는 Boss2D 쫄몹. */
UCLASS()
class TD_PROJECT_API ATDExploderMinion : public ATDEnemyBase
{
	GENERATED_BODY()

public:
	ATDExploderMinion();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** AI가 폭발 거리에서 호출한다. 서버에서만 실제 상태를 바꾼다. */
	void BeginExplosionFuse();

	UFUNCTION(BlueprintPure, Category = "TD|Exploder")
	bool IsExploding() const { return bExploding; }

	UFUNCTION(BlueprintPure, Category = "TD|Exploder")
	float GetExplosionTriggerRadius() const { return ExplosionTriggerRadius; }

	ATD2DBossCharacter* GetOwningBoss() const;

	UPROPERTY(BlueprintAssignable, Category = "TD|Exploder|Animation")
	FTDOnExploderExploded OnExploded;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleDeath() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Exploder", meta = (ClampMin = "0"))
	float ExplosionTriggerRadius = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Exploder", meta = (ClampMin = "0"))
	float ExplosionRadius = 250.f;

	/** DT_MonsterDefinition의 BaseDamage에 곱하는 값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Exploder", meta = (ClampMin = "0"))
	float ExplosionDamageScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Exploder", meta = (ClampMin = "0"))
	float ExplosionDestroyDelay = 0.15f;

	/** 길찾기 실패로 영원히 남는 것을 막는다. 0이면 제한 없음. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Exploder", meta = (ClampMin = "0"))
	float MaxLifetime = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Exploder|Feedback")
	TObjectPtr<UNiagaraSystem> ExplosionVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Exploder|Feedback")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Exploder|Animation")
	FTD2DDirectionalFlipbooks IdleAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Exploder|Animation")
	FTD2DDirectionalFlipbooks WalkAnimation;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExplode(FVector Location);

private:
	void Explode();
	void RefreshFlipbook();
	UPaperFlipbook* PickDirectionalFlipbook(const FTD2DDirectionalFlipbooks& Set) const;

	UPROPERTY(Replicated)
	bool bExploding = false;

	FVector VisualFacing = FVector::ForwardVector;

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> CurrentVisualFlipbook;
};
