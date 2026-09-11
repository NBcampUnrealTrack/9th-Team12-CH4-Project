#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "CoreMinimal.h"
#include "BTTask_BossChoosePattern.generated.h"

/** 거리·쿨·가중치로 다음 패턴을 골라 NextPattern 에 쓴다. 준비된 게 없으면 실패 → 트리가 접근으로 넘어간다. */
UCLASS()
class TD_PROJECT_API UBTTask_BossChoosePattern : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_BossChoosePattern();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};