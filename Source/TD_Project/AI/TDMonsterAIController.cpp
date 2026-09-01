#include "AI/TDMonsterAIController.h"

#include "Character/TDCharacterBase.h"
#include "Combat/TDCombatComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

void ATDMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// AI 판단은 서버 몫이다.
	if (!HasAuthority())
	{
		return;
	}

	ATDCharacterBase* PossessedCharacter = Cast<ATDCharacterBase>(InPawn);
	if (PossessedCharacter == nullptr)
	{
		return;
	}

	// 컨트롤러의 팀을 몸의 팀과 맞춘다. 나중에 Perception 을 붙여도 이 값이 기준이 된다.
	SetGenericTeamId(PossessedCharacter->GetGenericTeamId());

	// 죽으면 생각을 멈춘다 — OnDeath 소켓이 설계된 용도 그대로다.
	PossessedCharacter->OnDeath.AddDynamic(this, &ATDMonsterAIController::HandlePawnDeath);

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

void ATDMonsterAIController::Think()
{
	ATDCharacterBase* Self = Cast<ATDCharacterBase>(GetPawn());
	if (Self == nullptr || Self->IsDead())
	{
		return;
	}

	// ── 어그로 갱신 ──
	ATDCharacterBase* Target = AggroTarget.Get();

	// 대상이 사라졌거나, 죽었거나, 너무 멀어졌으면 포기한다.
	if (Target != nullptr)
	{
		const float Distance = FVector::Dist(Self->GetActorLocation(), Target->GetActorLocation());
		if (Target->IsDead() || Distance > LoseAggroRadius)
		{
			AggroTarget = nullptr;
			Target = nullptr;
			StopMovement();
			ClearFocus(EAIFocusPriority::Gameplay);
		}
	}

	// 대상이 없으면 새로 찾는다.
	if (Target == nullptr)
	{
		Target = FindNearestEnemy();
		AggroTarget = Target;

		if (Target == nullptr)
		{
			return;   // 대기 상태. TODO: 스폰 지점 귀환·배회는 스폰 시스템과 함께 붙인다.
		}
	}

	// ── 추적 / 공격 분기 ──
	const float Distance = FVector::Dist(Self->GetActorLocation(), Target->GetActorLocation());

	// 대상을 계속 바라본다. 히트박스가 전방 판정이므로 이게 곧 조준이다.
	SetFocus(Target, EAIFocusPriority::Gameplay);

	if (Distance <= AttackRange)
	{
		StopMovement();

		// 쿨타임은 CombatComponent 의 CanAttack 이 관리한다. 매 판단마다 눌러도 안전하다.
		if (UTDCombatComponent* Combat = Self->GetCombatComponent())
		{
			Combat->ServerRequestAttack();
		}
	}
	else
	{
		// 사거리보다 살짝 안쪽까지 접근해야 멈춘 자리에서 히트박스가 닿는다.
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

		// 같은 팀은 적이 아니다. 팀 값은 BP 에서 지정한다(플레이어 0 / 몬스터 1).
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