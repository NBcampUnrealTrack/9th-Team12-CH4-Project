#include "AI/TDMonsterAIController.h"

#include "Character/TDCharacterBase.h"
#include "Character/TDEnemyBase.h"
#include "Combat/TDCombatComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

void ATDMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!HasAuthority())
	{
		return;
	}

	ATDCharacterBase* PossessedCharacter = Cast<ATDCharacterBase>(InPawn);
	if (PossessedCharacter == nullptr)
	{
		return;
	}

	SetGenericTeamId(PossessedCharacter->GetGenericTeamId());
	PossessedCharacter->OnDeath.AddDynamic(this, &ATDMonsterAIController::HandlePawnDeath);

	// 배회의 기준점. 스포너가 놓아준 자리가 곧 집이다.
	HomeLocation = InPawn->GetActorLocation();

	EnterIdle();

	GetWorldTimerManager().SetTimer(
		ThinkTimerHandle, this, &ATDMonsterAIController::Think, ThinkInterval, true);
}

void ATDMonsterAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(ThinkTimerHandle);
	Super::OnUnPossess();
}

void ATDMonsterAIController::HandlePawnDeath()
{
	GetWorldTimerManager().ClearTimer(ThinkTimerHandle);
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
}

// ── 상태 진입 ─────────────────────────────────────────────

void ATDMonsterAIController::EnterIdle()
{
	State = ETDMonsterAIState::Idle;
	StateEndTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(IdleTimeMin, IdleTimeMax);
	AggroTarget = nullptr;
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
}

void ATDMonsterAIController::EnterWander()
{
	// NavMesh 위의 도달 가능한 지점만 고른다. 실패하면(포인트가 초록 영역 밖 등) 그냥 더 쉰다.
	FNavLocation WanderPoint;
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	if (NavSystem == nullptr ||
		!NavSystem->GetRandomReachablePointInRadius(HomeLocation, WanderRadius, WanderPoint))
	{
		EnterIdle();
		return;
	}

	State = ETDMonsterAIState::Wander;
	StateEndTime = GetWorld()->GetTimeSeconds() + WanderTimeLimit;
	MoveToLocation(WanderPoint.Location, 50.f);
}

void ATDMonsterAIController::EnterSense(ATDCharacterBase* Found)
{
	State = ETDMonsterAIState::Sense;
	StateEndTime = GetWorld()->GetTimeSeconds() + SenseDuration;
	AggroTarget = Found;

	StopMovement();
	SetFocus(Found, EAIFocusPriority::Gameplay);   // 굳은 채로 적을 바라본다

	// "발견!" 연출 신호. 애니메이션은 BP 가 구독해서 재생한다.
	if (ATDEnemyBase* Enemy = Cast<ATDEnemyBase>(GetPawn()))
	{
		Enemy->MulticastOnSense();
	}
}

void ATDMonsterAIController::EnterCombat()
{
	State = ETDMonsterAIState::Combat;
}

// ── 주기 판단 ─────────────────────────────────────────────

void ATDMonsterAIController::Think()
{
	ATDCharacterBase* Self = Cast<ATDCharacterBase>(GetPawn());
	if (Self == nullptr || Self->IsDead())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	switch (State)
	{
	case ETDMonsterAIState::Idle:
		// 평화 시엔 적 탐지가 최우선이다.
		if (ATDCharacterBase* Found = FindNearestEnemy())
		{
			EnterSense(Found);
			return;
		}
		if (Now >= StateEndTime)
		{
			EnterWander();
		}
		return;

	case ETDMonsterAIState::Wander:
		if (ATDCharacterBase* Found = FindNearestEnemy())
		{
			EnterSense(Found);
			return;
		}
		// 도착했거나(경로 추적이 놀고 있음) 너무 오래 걸리면 대기로.
		if (GetMoveStatus() == EPathFollowingStatus::Idle || Now >= StateEndTime)
		{
			EnterIdle();
		}
		return;

	case ETDMonsterAIState::Sense:
		// 굳어 있는 동안은 아무것도 안 한다. 시간이 차면 전투로.
		if (Now >= StateEndTime)
		{
			EnterCombat();
		}
		return;

	case ETDMonsterAIState::Combat:
		TickCombat(Self);
		return;
	}
}

void ATDMonsterAIController::TickCombat(ATDCharacterBase* Self)
{
	ATDCharacterBase* Target = AggroTarget.Get();

	// 대상 상실 — 죽었거나, 사라졌거나, 너무 멀어졌다.
	if (Target == nullptr || Target->IsDead() ||
		FVector::Dist(Self->GetActorLocation(), Target->GetActorLocation()) > LoseAggroRadius)
	{
		EnterIdle();
		return;
	}

	const float Distance = FVector::Dist(Self->GetActorLocation(), Target->GetActorLocation());

	SetFocus(Target, EAIFocusPriority::Gameplay);

	if (Distance <= AttackRange)
	{
		StopMovement();

		if (UTDCombatComponent* Combat = Self->GetCombatComponent())
		{
			Combat->ServerRequestAttack();
		}
	}
	else
	{
		MoveToActor(Target, AttackRange * 0.7f);
	}
}

ATDCharacterBase* ATDMonsterAIController::FindNearestEnemy() const
{
	const APawn* Self = GetPawn();
	const ATDCharacterBase* SelfCharacter = Cast<ATDCharacterBase>(Self);
	if (SelfCharacter == nullptr)
	{
		return nullptr;
	}

	ATDCharacterBase* Nearest = nullptr;
	float NearestDistSq = FMath::Square(AggroRadius);

	for (TActorIterator<ATDCharacterBase> It(GetWorld()); It; ++It)
	{
		ATDCharacterBase* Candidate = *It;
		if (Candidate == nullptr || Candidate == SelfCharacter || Candidate->IsDead())
		{
			continue;
		}

		if (Candidate->GetGenericTeamId() == SelfCharacter->GetGenericTeamId())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(
			Self->GetActorLocation(), Candidate->GetActorLocation());

		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Candidate;
		}
	}

	return Nearest;
}