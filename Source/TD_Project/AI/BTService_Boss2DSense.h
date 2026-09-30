#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_Boss2DSense.generated.h"

/** BT_Boss2D 전용 감지. 기존 Boss Sense와 함께 붙이지 않는다. */
UCLASS()
class TD_PROJECT_API UBTService_Boss2DSense : public UBTService
{
	GENERATED_BODY()
public:
	UBTService_Boss2DSense();
protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
