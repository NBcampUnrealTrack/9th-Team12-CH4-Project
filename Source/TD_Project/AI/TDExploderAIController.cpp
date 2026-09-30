#include "AI/TDExploderAIController.h"

#include "Character/Boss2D/TD2DBossCharacter.h"
#include "Character/TDCharacterBase.h"
#include "Character/Boss2D/TDExploderMinion.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"

void ATDExploderAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!HasAuthority())
	{
		return;
	}

	ATDExploderMinion* Minion = Cast<ATDExploderMinion>(InPawn);
	if (Minion == nullptr)
	{
		return;
	}
	SetGenericTeamId(Minion->GetGenericTeamId());
	Minion->OnDeath.AddDynamic(this, &ATDExploderAIController::HandlePawnDeath);
	GetWorldTimerManager().SetTimer(ThinkTimerHandle, this, &ATDExploderAIController::Think,
		ThinkInterval, true, 0.f);
}

void ATDExploderAIController::OnUnPossess()
{
	if (ATDExploderMinion* Minion = Cast<ATDExploderMinion>(GetPawn()))
	{
		Minion->OnDeath.RemoveDynamic(this, &ATDExploderAIController::HandlePawnDeath);
	}
	GetWorldTimerManager().ClearTimer(ThinkTimerHandle);
	Super::OnUnPossess();
}

void ATDExploderAIController::HandlePawnDeath()
{
	GetWorldTimerManager().ClearTimer(ThinkTimerHandle);
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
}

void ATDExploderAIController::Think()
{
	ATDExploderMinion* Minion = Cast<ATDExploderMinion>(GetPawn());
	if (Minion == nullptr || Minion->IsDead() || Minion->IsExploding())
	{
		return;
	}

	ATDCharacterBase* CurrentTarget = Target.Get();
	ATD2DBossCharacter* Boss = Minion->GetOwningBoss();
	const bool bCurrentValid = CurrentTarget != nullptr && !CurrentTarget->IsDead()
		&& CurrentTarget->GetGenericTeamId() != Minion->GetGenericTeamId()
		&& (Boss == nullptr || Boss->IsLocationInsideArena(CurrentTarget->GetActorLocation()));

	if (!bCurrentValid)
	{
		CurrentTarget = Boss ? Boss->FindNearestEnemyInArena() : FindFallbackTarget();
		Target = CurrentTarget;
	}
	if (CurrentTarget == nullptr || (Boss != nullptr && !Boss->IsFightActive()))
	{
		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}

	const float SelfRadius = Minion->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float TargetRadius = CurrentTarget->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float EdgeDistance = FVector::Dist2D(Minion->GetActorLocation(), CurrentTarget->GetActorLocation())
		- SelfRadius - TargetRadius;

	SetFocus(CurrentTarget, EAIFocusPriority::Gameplay);
	if (EdgeDistance <= Minion->GetExplosionTriggerRadius())
	{
		StopMovement();
		Minion->BeginExplosionFuse();
		return;
	}

	MoveToActor(CurrentTarget, Minion->GetExplosionTriggerRadius() * 0.7f);
}

ATDCharacterBase* ATDExploderAIController::FindFallbackTarget() const
{
	const ATDCharacterBase* Self = Cast<ATDCharacterBase>(GetPawn());
	if (Self == nullptr)
	{
		return nullptr;
	}

	ATDCharacterBase* Best = nullptr;
	float BestDistanceSq = FMath::Square(FallbackSearchRadius);
	for (TActorIterator<ATDCharacterBase> It(GetWorld()); It; ++It)
	{
		ATDCharacterBase* Candidate = *It;
		if (Candidate == nullptr || Candidate == Self || Candidate->IsDead()
			|| Candidate->GetGenericTeamId() == Self->GetGenericTeamId())
		{
			continue;
		}
		const float DistanceSq = FVector::DistSquared2D(Self->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Candidate;
		}
	}
	return Best;
}
