#pragma once

#include "AIController.h"
#include "CoreMinimal.h"
#include "TDExploderAIController.generated.h"

class ATDCharacterBase;
class ATDExploderMinion;

/** Idle/Walk 두 상태만 필요한 자폭 쫄몹의 가벼운 서버 AI. */
UCLASS()
class TD_PROJECT_API ATDExploderAIController : public AAIController
{
	GENERATED_BODY()

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UPROPERTY(EditDefaultsOnly, Category = "TD|Exploder AI", meta = (ClampMin = "0.05"))
	float ThinkInterval = 0.15f;

	/** 보스 Owner가 없는 테스트 배치에서만 쓰는 탐색 범위. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Exploder AI", meta = (ClampMin = "0"))
	float FallbackSearchRadius = 2500.f;

private:
	void Think();
	ATDCharacterBase* FindFallbackTarget() const;

	UFUNCTION()
	void HandlePawnDeath();

	FTimerHandle ThinkTimerHandle;
	TWeakObjectPtr<ATDCharacterBase> Target;
};
