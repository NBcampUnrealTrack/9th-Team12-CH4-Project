#pragma once

#include "AIController.h"
#include "CoreMinimal.h"
#include "TDBossAIController.generated.h"

class ATDBossCharacter;
class UBehaviorTree;

/**
 * 보스의 두뇌. 빙의하면 BT 를 돌린다. 판단은 트리가, 실행은 보스 클래스가.
 * 필드 몬스터 FSM 컨트롤러와 별개 — 보스는 배회·발견이 없고 패턴 선택이 핵심이라 트리가 맞다.
 */
UCLASS()
class TD_PROJECT_API ATDBossAIController : public AAIController
{
	GENERATED_BODY()

public:
	ATDBossCharacter* GetBoss() const;
	float GetEngageRadius() const { return EngageRadius; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** BP 자식에서 BT_Boss 를 지정한다. C++ 에서 에셋을 참조하지 않는 컨벤션. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Boss AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	/** 이 거리 안에 적이 들어오면 전투 시작(입장 연출). 4단계에서 존 진입 트리거로 대체 가능. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Boss AI", meta = (ClampMin = "0"))
	float EngageRadius = 1500.f;

	UFUNCTION()
	void HandlePawnDeath();
};