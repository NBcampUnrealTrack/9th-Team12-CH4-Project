#pragma once

#include "BehaviorTree/BTService.h"
#include "CoreMinimal.h"
#include "BTService_BossSense.generated.h"

/**
 * 보스의 감각. 0.2초마다: 대상 선택·유지 → 전투 시작/종료 → 리시 → 블랙보드 기록.
 * 트리의 루트 Selector 에 붙인다. 판단 재료를 만드는 곳이지 행동을 정하는 곳이 아니다.
 */
UCLASS()
class TD_PROJECT_API UBTService_BossSense : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_BossSense();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};