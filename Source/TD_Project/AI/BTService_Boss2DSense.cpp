#include "AI/BTService_Boss2DSense.h"

#include "AI/TDBossAITypes.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Boss2D/TD2DBossCharacter.h"

UBTService_Boss2DSense::UBTService_Boss2DSense()
{
	NodeName = TEXT("Boss 2D Arena Sense");
	Interval = 0.2f;
	RandomDeviation = 0.f;
}

void UBTService_Boss2DSense::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	AAIController* AIC = OwnerComp.GetAIOwner();
	ATD2DBossCharacter* Boss = AIC ? Cast<ATD2DBossCharacter>(AIC->GetPawn()) : nullptr;
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!Boss || !BB || Boss->IsDead()) return;

	BB->SetValueAsVector(TDBossBB::HomeLocation, Boss->GetHomeNavLocation());

	ATDCharacterBase* Target = Cast<ATDCharacterBase>(BB->GetValueAsObject(TDBossBB::Target));
	if (Boss->IsReturning())
	{
		Target = nullptr;
	}
	else
	{
		const bool bTargetValid = IsValid(Target) && !Target->IsDead()
			&& Target->GetGenericTeamId() != Boss->GetGenericTeamId()
			&& Boss->IsLocationInsideArena(Target->GetActorLocation());
		if (!bTargetValid) Target = Boss->FindNearestEnemyInArena();

		if (Target && !Boss->IsFightActive()) Boss->BeginFight(Target);
		else if (!Target && Boss->IsFightActive()) Boss->ResetArenaFight();
	}

	Boss->SetPatternTarget(Target);
	const float Distance = Target ? FVector::Dist2D(Boss->GetActorLocation(), Target->GetActorLocation()) : 0.f;
	if (Target)
	{
		BB->SetValueAsObject(TDBossBB::Target, Target);
	}
	else
	{
		BB->ClearValue(TDBossBB::Target);
	}
	BB->SetValueAsFloat(TDBossBB::Distance, Distance);
	BB->SetValueAsInt(TDBossBB::Phase, Boss->GetPhase());
	BB->SetValueAsBool(TDBossBB::bReturning, Boss->IsReturning());
	BB->SetValueAsBool(TDBossBB::bBusy, Boss->IsArenaBusy());
	BB->SetValueAsBool(TDBossBB::bFightActive, Boss->IsFightActive());
	BB->SetValueAsBool(TDBossBB::bPatternReady,
		Target && !Boss->IsArenaBusy() && Boss->HasReadyPattern(Distance));
}
