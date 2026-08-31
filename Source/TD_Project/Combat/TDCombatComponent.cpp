#include "Combat/TDCombatComponent.h"

#include "Character/TDCharacterBase.h"
#include "Combat/TDCombatStatics.h"
#include "Core/TDGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

UTDCombatComponent::UTDCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTDCombatComponent::ServerRequestAttack_Implementation()
{
	ATDCharacterBase* Owner = Cast<ATDCharacterBase>(GetOwner());
	if (Owner == nullptr || !CanAttack())
	{
		return;
	}

	LastAttackTime = GetWorld()->GetTimeSeconds();

	// 모션은 즉시 시작하고, 판정은 휘두르는 순간까지 미룬다.
	MulticastOnAttack();

	if (HitDelay <= 0.f)
	{
		PerformHit();
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		HitTimerHandle, this, &UTDCombatComponent::PerformHit, HitDelay, false);
	
}

void UTDCombatComponent::PerformHit()
{
	ATDCharacterBase* Owner = Cast<ATDCharacterBase>(GetOwner());

	// 휘두르는 도중에 죽었으면 스윙은 무효다.
	if (Owner == nullptr || Owner->IsDead())
	{
		return;
	}

	for (AActor* Target : GatherTargets())
	{
		const FTDDamageResult Result = UTDCombatStatics::ApplyDamage(
			Owner, Target, FGameplayTagContainer());

		if (Result.FinalDamage > 0.f)
		{
			MulticastOnHit(Target, Result.FinalDamage, Result.bCritical);
		}
	}
}

void UTDCombatComponent::MulticastOnAttack_Implementation()
{
	OnAttackStarted.Broadcast();
}

bool UTDCombatComponent::CanAttack() const
{
	const ATDCharacterBase* Owner = Cast<ATDCharacterBase>(GetOwner());
	if (Owner == nullptr || Owner->IsDead())
	{
		return false;
	}

	// 쿨다운회복률: 실제 쿨타임 = 기본 ÷ (1 + 회복률). 시전 시점에 1회 계산한다.
	const float RecoveryRate = FMath::Max(0.f, Owner->GetStat(TDTags::Stat_Utility_CooldownRecoveryRate));
	const float ActualCooldown = AttackCooldown / (1.f + RecoveryRate);

	return GetWorld()->GetTimeSeconds() - LastAttackTime >= ActualCooldown;
}

TArray<AActor*> UTDCombatComponent::GatherTargets() const
{
	TArray<AActor*> Targets;

	const ATDCharacterBase* Owner = Cast<ATDCharacterBase>(GetOwner());
	if (Owner == nullptr)
	{
		return Targets;
	}

	// 전방 박스. 지금은 액터의 정면 벡터를 쓴다 —
	// 스프라이트 좌우 반전과의 동기화는 애님 인계 후 여기만 고치면 된다.
	const FVector Center = Owner->GetActorLocation()
		+ Owner->GetActorForwardVector() * HitBoxForwardOffset;
	const FCollisionShape Box = FCollisionShape::MakeBox(HitBoxExtent);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, Owner->GetActorQuat(),
		FCollisionObjectQueryParams(ECC_Pawn), Box, Params);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugHitBox)
	{
		DrawDebugBox(GetWorld(), Center, HitBoxExtent, Owner->GetActorQuat(),
			FColor::Red, false, 0.5f);
	}
#endif

	// 한 번의 공격에 같은 대상이 두 번 잡히지 않도록 걸러 담는다.
	TSet<AActor*> Seen;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ATDCharacterBase* Candidate = Cast<ATDCharacterBase>(Overlap.GetActor());
		if (Candidate == nullptr || Candidate->IsDead() || Seen.Contains(Candidate))
		{
			continue;
		}

		// 같은 팀은 때리지 않는다(§10-④ 임시 규칙).
		if (Candidate->GetGenericTeamId() == Owner->GetGenericTeamId())
		{
			continue;
		}

		Seen.Add(Candidate);
		Targets.Add(Candidate);
	}

	return Targets;
}

void UTDCombatComponent::MulticastOnHit_Implementation(AActor* Target, float Damage, bool bCritical)
{
	OnHit.Broadcast(Target, Damage, bCritical);
}