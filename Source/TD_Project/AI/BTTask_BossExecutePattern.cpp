#include "AI/BTTask_BossExecutePattern.h"

#include "AI/TDBossAIController.h"
#include "AI/TDBossAITypes.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/TDBossCharacter.h"
#include "Character/TDCharacterBase.h"

UBTTask_BossExecutePattern::UBTTask_BossExecutePattern()
{
	NodeName = TEXT("Boss Execute Pattern");
	bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_BossExecutePattern::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ATDBossAIController* AIC = Cast<ATDBossAIController>(OwnerComp.GetAIOwner());
	ATDBossCharacter* Boss = AIC ? AIC->GetBoss() : nullptr;
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (Boss == nullptr || BB == nullptr)
	{
		return EBTNodeResult::Failed;
	}

	const int32 Index = BB->GetValueAsInt(TDBossBB::NextPattern);
	ATDCharacterBase* Target = Cast<ATDCharacterBase>(BB->GetValueAsObject(TDBossBB::Target));

	// 최원거리 타깃: 잠수는 가장 먼 적을 노린다. 원거리 직업이 뒤에서 안전하게 서 있지 못하게.
	FTDBossPatternSpec Spec;
	if (Boss->GetPatternSpec(Index, Spec) && Spec.Motion == ETDBossMotion::Burrow)
	{
		if (ATDCharacterBase* Farthest = Boss->FindEnemy(true, Boss->GetLeashRadius()))
		{
			Target = Farthest;
		}
	}
	Boss->SetPatternTarget(Target);

	FBTBossExecutePatternMemory* Memory = reinterpret_cast<FBTBossExecutePatternMemory*>(NodeMemory);
	Memory->Boss = Boss;
	Memory->PatternIndex = Index;
	Memory->FinishedHandle = Boss->OnPatternFinished.AddUObject(
		this, &UBTTask_BossExecutePattern::HandlePatternFinished, TWeakObjectPtr<UBehaviorTreeComponent>(&OwnerComp));

	if (!Boss->StartPattern(Index))
	{
		Unbind(Memory);
		return EBTNodeResult::Failed;   // 바쁨·사망 — 트리가 다음 가지로
	}
	return EBTNodeResult::InProgress;
}

void UBTTask_BossExecutePattern::HandlePatternFinished(int32 PatternIndex, TWeakObjectPtr<UBehaviorTreeComponent> OwnerComp)
{
	if (OwnerComp.IsValid())
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_BossExecutePattern::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTBossExecutePatternMemory* Memory = reinterpret_cast<FBTBossExecutePatternMemory*>(NodeMemory);
	if (Memory->Boss.IsValid())
	{
		Memory->Boss->CancelPattern();
	}
	Unbind(Memory);
	return EBTNodeResult::Aborted;
}

void UBTTask_BossExecutePattern::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	Unbind(reinterpret_cast<FBTBossExecutePatternMemory*>(NodeMemory));
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

void UBTTask_BossExecutePattern::Unbind(FBTBossExecutePatternMemory* Memory)
{
	if (Memory->Boss.IsValid() && Memory->FinishedHandle.IsValid())
	{
		Memory->Boss->OnPatternFinished.Remove(Memory->FinishedHandle);
	}
	Memory->FinishedHandle.Reset();
}