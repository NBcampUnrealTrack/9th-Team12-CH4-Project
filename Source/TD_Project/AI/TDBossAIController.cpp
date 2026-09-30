#include "AI/TDBossAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BrainComponent.h"
#include "Character/TDBossCharacter.h"

ATDBossCharacter* ATDBossAIController::GetBoss() const
{
	return Cast<ATDBossCharacter>(GetPawn());
}

void ATDBossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!HasAuthority())
	{
		return;
	}

	ATDBossCharacter* Boss = Cast<ATDBossCharacter>(InPawn);
	if (Boss == nullptr)
	{
		return;
	}

	SetGenericTeamId(Boss->GetGenericTeamId());
	Boss->OnDeath.AddDynamic(this, &ATDBossAIController::HandlePawnDeath);

	if (BehaviorTreeAsset == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: BehaviorTreeAsset 이 비어 있어 보스가 생각하지 않는다."), *GetName());
		return;
	}
	RunBehaviorTree(BehaviorTreeAsset);   // 트리에 연결된 블랙보드도 함께 초기화된다
}

void ATDBossAIController::OnUnPossess()
{
	if (ATDBossCharacter* Boss = GetBoss())
	{
		Boss->OnDeath.RemoveDynamic(this, &ATDBossAIController::HandlePawnDeath);
	}
	Super::OnUnPossess();
}

void ATDBossAIController::HandlePawnDeath()
{
	if (BrainComponent != nullptr)
	{
		BrainComponent->StopLogic(TEXT("Dead"));
	}
	StopMovement();
}