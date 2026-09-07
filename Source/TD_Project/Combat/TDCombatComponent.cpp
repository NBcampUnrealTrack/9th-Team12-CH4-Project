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
	
	// Multicast RPC(공격·피격 방송)가 클라이언트까지 가려면 컴포넌트 자신도 복제 대상이어야 한다.
	// 이게 없으면 리슨 서버 화면에서만 연출이 보이고 클라 창에서는 조용히 증발한다.
	SetIsReplicatedByDefault(true);
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
			// 접촉점 근사치: 공격자에서 가장 가까운 대상 콜리전 표면의 점.
			// 순간 질의라 진짜 충돌점은 없지만, 칼이 닿았을 법한 위치로는 충분하다.
			FVector HitLocation = Target->GetActorLocation();   // 실패 시 폴백 = 몸 중심

			if (UPrimitiveComponent* TargetRoot = Cast<UPrimitiveComponent>(Target->GetRootComponent()))
			{
				FVector ClosestPoint;
				if (TargetRoot->GetClosestPointOnCollision(Owner->GetActorLocation(), ClosestPoint) >= 0.f)
				{
					HitLocation = ClosestPoint;
				}
			}

			MulticastOnHit(Target, Result.FinalDamage, Result.bCritical, HitLocation);
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

void UTDCombatComponent::MulticastOnHit_Implementation(AActor* Target, float Damage, bool bCritical, FVector HitLocation)
{
	OnHit.Broadcast(Target, Damage, bCritical, HitLocation);
}