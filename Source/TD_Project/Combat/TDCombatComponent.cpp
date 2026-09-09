#include "Combat/TDCombatComponent.h"

#include "Character/TDCharacterBase.h"
#include "Combat/TDCombatStatics.h"
#include "Core/TDGameplayTags.h"
#include "Engine/World.h"
#include "Interaction/TDInteractionFlowComponent.h"
#include "GameFramework/PlayerController.h"
#include "Skill/TDSkillComponent.h"
#include "GameFramework/Character.h"

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

	// 경직 중엔 못 친다. 시전 직후 맞으면 CancelAttack 이 스윙을 이미 끊었다.
	if (Owner->IsStaggered())
	{
		return false;
	}
	
	//추가- "플레이어가 대화 중이거나 컷씬을 보는 중이면, 쿨다운이 다 됐어도 공격을 못 하게 막는다"
	const APlayerController* Controller = Cast<APlayerController>(Owner->GetController());
	const UTDInteractionFlowComponent* Flow =
			Controller
				? Controller->FindComponentByClass<
					UTDInteractionFlowComponent>()
				: nullptr;
	if (Flow != nullptr
			&& Flow->IsGameplayLocked())
	{
		return false;
	}

	// 시전 중에는 평타가 나가지 않는다. 막지 않으면 정신집중으로 계속 때리면서
	// 평타까지 섞어 넣을 수 있다.
	const UTDSkillComponent* Skills = Owner->FindComponentByClass<UTDSkillComponent>();
	if (Skills != nullptr && Skills->IsCasting())
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
	// 규칙 본체는 UTDCombatStatics 로 옮겼다. 스킬이 같은 판정을 써야 하는데
	// 여기 두면 복사본이 하나 더 생긴다.
	//
	// 방향은 액터 회전이 아니라 마지막 이동 방향이다 — 스프라이트 방향
	// (ABP SetDirectionality ← Velocity)과 같은 출처라 눈에 보이는 쪽과 일치한다.
	return UTDCombatStatics::GatherTargetsInBox(
		GetOwner(), FacingDirection, HitBoxExtent, HitBoxForwardOffset, bDrawDebugHitBox);
}

void UTDCombatComponent::NotifyHit(AActor* Target, float Damage, bool bCritical, FVector HitLocation)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		MulticastOnHit(Target, Damage, bCritical, HitLocation);
	}
}

void UTDCombatComponent::MulticastOnHit_Implementation(AActor* Target, float Damage, bool bCritical, FVector HitLocation)
{
	OnHit.Broadcast(Target, Damage, bCritical, HitLocation);
}

void UTDCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (OwnerCharacter == nullptr)
	{
		return;
	}

	FacingDirection = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();

	// 이동 컴포넌트가 움직임을 적용할 때마다 캐릭터가 방송하는 엔진 델리게이트.
	// Tick 을 켜지 않고도 방향을 따라간다. 서버도 이동을 시뮬레이션하므로 서버에서도 불린다.
	OwnerCharacter->OnCharacterMovementUpdated.AddDynamic(
		this, &UTDCombatComponent::HandleMovementUpdated);
}

void UTDCombatComponent::HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	if (const AActor* Owner = GetOwner())
	{
		SetFacingDirection(Owner->GetVelocity());
	}
}

void UTDCombatComponent::SetFacingDirection(const FVector& Direction)
{
	// 미세한 미끄러짐으로 방향이 튀지 않게 문턱을 둔다. 2.5D 라 Z 는 버린다.
	const FVector Flat(Direction.X, Direction.Y, 0.f);
	if (Flat.SizeSquared() < FMath::Square(5.f))
	{
		return;
	}
	FacingDirection = Flat.GetSafeNormal();
}

void UTDCombatComponent::CancelAttack()
{
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(HitTimerHandle);
	}
}