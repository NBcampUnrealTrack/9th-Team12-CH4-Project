#pragma once

#include "CoreMinimal.h"
#include "AI/TDBossAIController.h"
#include "TD2DBossAIController.generated.h"

/** 새 BT가 전투를 맡고, 대상이 없을 때 원 안 배회만 보조하는 2D 보스 컨트롤러. */
UCLASS()
class TD_PROJECT_API ATD2DBossAIController : public ATDBossAIController
{
	GENERATED_BODY()

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UPROPERTY(EditDefaultsOnly, Category = "TD|2D Boss AI|Wander", meta = (ClampMin = "0.1"))
	float WanderThinkInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "TD|2D Boss AI|Wander", meta = (ClampMin = "0"))
	float IdleTimeMin = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "TD|2D Boss AI|Wander", meta = (ClampMin = "0"))
	float IdleTimeMax = 3.f;

private:
	void TickWander();
	FTimerHandle WanderTimerHandle;
	float NextWanderTime = 0.f;
};
