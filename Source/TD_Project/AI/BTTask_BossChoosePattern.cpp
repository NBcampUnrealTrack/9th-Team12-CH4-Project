#include "AI/BTTask_BossChoosePattern.h"

#include "AI/TDBossAIController.h"
#include "AI/TDBossAITypes.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/TDBossCharacter.h"

UBTTask_BossChoosePattern::UBTTask_BossChoosePattern()
{
	NodeName = TEXT("Boss Choose Pattern");
}

EBTNodeResult::Type UBTTask_BossChoosePattern::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ATDBossAIController* AIC = Cast<ATDBossAIController>(OwnerComp.GetAIOwner());
	ATDBossCharacter* Boss = AIC ? AIC->GetBoss() : nullptr;
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (Boss == nullptr || BB == nullptr || Boss->IsBusy())
	{
		return EBTNodeResult::Failed;
	}

	const int32 Index = Boss->ChoosePattern(BB->GetValueAsFloat(TDBossBB::Distance));
	if (Index == INDEX_NONE)
	{
		return EBTNodeResult::Failed;   // 전부 쿨이거나 거리 조건 밖
	}

	BB->SetValueAsInt(TDBossBB::NextPattern, Index);
	return EBTNodeResult::Succeeded;
}