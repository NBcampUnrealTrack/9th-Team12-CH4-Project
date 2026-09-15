#include "AI/TD2DBossAIController.h"

#include "Character/Boss2D/TD2DBossCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

void ATD2DBossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!HasAuthority() || Cast<ATD2DBossCharacter>(InPawn) == nullptr)
	{
		return;
	}

	NextWanderTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(WanderTimerHandle, this, &ATD2DBossAIController::TickWander,
		WanderThinkInterval, true, 0.f);
}

void ATD2DBossAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(WanderTimerHandle);
	Super::OnUnPossess();
}

void ATD2DBossAIController::TickWander()
{
	ATD2DBossCharacter* Boss = Cast<ATD2DBossCharacter>(GetPawn());
	if (Boss == nullptr || Boss->IsDead() || Boss->IsFightActive() || Boss->IsReturning() || Boss->IsBusy())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (GetMoveStatus() != EPathFollowingStatus::Idle || Now < NextWanderTime)
	{
		return;
	}

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSystem == nullptr)
	{
		NextWanderTime = Now + 1.f;
		return;
	}

	const float CapsuleRadius = Boss->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float SearchRadius = FMath::Max(100.f, Boss->GetArenaRadius() - CapsuleRadius - 50.f);
	FNavLocation Candidate;
	if (!NavSystem->GetRandomReachablePointInRadius(Boss->GetHomeNavLocation(), SearchRadius, Candidate))
	{
		NextWanderTime = Now + 1.f;
		return;
	}

	const FVector Goal = Boss->ClampLocationToArena(Candidate.Location, 25.f);
	MoveToLocation(Goal, 50.f);
	NextWanderTime = Now + FMath::FRandRange(IdleTimeMin, FMath::Max(IdleTimeMin, IdleTimeMax));
}
