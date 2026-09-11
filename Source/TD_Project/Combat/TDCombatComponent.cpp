#include "Combat/TDCombatComponent.h"

#include "Character/TDCharacterBase.h"
#include "Combat/TDCombatStatics.h"
#include "Core/TDGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TDInteractionFlowComponent.h"

UTDCombatComponent::UTDCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Multicast RPC(공격·피격 방송)가 클라이언트까지 가려면 컴포넌트 자신도 복제 대상이어야 한다.
	SetIsReplicatedByDefault(true);

	// 기본 평타 1종. BP 가 배열을 덮어쓰지 않으면 이 값이 그대로 쓰인다.
	Attacks.Add(FTDAttackSpec());
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
	PendingAttackIndex = INDEX_NONE;
}

// ── 공격 요청 ─────────────────────────────────────────────

void UTDCombatComponent::ServerRequestAttack_Implementation()
{
	RequestAttack(ChooseAttackIndex());
}

int32 UTDCombatComponent::ChooseAttackIndex() const
{
	if (Attacks.Num() <= 1)
	{
		return Attacks.Num() - 1;   // 1종이면 0, 없으면 INDEX_NONE
	}

	int32 Index = FMath::RandRange(0, Attacks.Num() - 1);

	// 직전과 같으면 한 칸 옆으로. 2종이면 번갈아, 3종 이상이면 "연속만 아닌" 랜덤이 된다.
	if (bAvoidRepeatingAttack && Index == LastAttackIndex)
	{
		Index = (Index + 1) % Attacks.Num();
	}
	return Index;
}

void UTDCombatComponent::RequestAttack(int32 AttackIndex)
{
	ATDCharacterBase* Owner = Cast<ATDCharacterBase>(GetOwner());
	if (Owner == nullptr || !Owner->HasAuthority() || !Attacks.IsValidIndex(AttackIndex) || !CanAttack())
	{
		return;
	}

	const FTDAttackSpec& Spec = Attacks[AttackIndex];

	// 쿨다운회복률: 실제 쿨타임 = 기본 ÷ (1 + 회복률). 시전 시점에 1회 계산해 둔다.
	const float RecoveryRate = FMath::Max(0.f, Owner->GetStat(TDTags::Stat_Utility_CooldownRecoveryRate));
	CurrentCooldown = Spec.Cooldown / (1.f + RecoveryRate);
	LastAttackTime = GetWorld()->GetTimeSeconds();
	LastAttackIndex = AttackIndex;
	PendingAttackIndex = AttackIndex;

	// 모션은 즉시 시작하고(번호 동봉), 판정은 휘두르는 순간까지 미룬다.
	MulticastOnAttack(AttackIndex);

	if (Spec.HitDelay <= 0.f)
	{
		PerformHit();
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		HitTimerHandle, this, &UTDCombatComponent::PerformHit, Spec.HitDelay, false);
}

bool UTDCombatComponent::CanAttack() const
{
	const ATDCharacterBase* Owner = Cast<ATDCharacterBase>(GetOwner());
	if (Owner == nullptr || Owner->IsDead() || Attacks.Num() == 0)
	{
		return false;
	}

	// 경직 중엔 못 친다. 시전 직후 맞으면 CancelAttack 이 스윙을 이미 끊었다.
	if (Owner->IsStaggered())
	{
		return false;
	}

	// 플레이어가 대화 중이거나 컷씬을 보는 중이면, 쿨다운이 다 됐어도 공격을 못 하게 막는다.
	const APlayerController* Controller = Cast<APlayerController>(Owner->GetController());
	const UTDInteractionFlowComponent* Flow =
		Controller ? Controller->FindComponentByClass<UTDInteractionFlowComponent>() : nullptr;
	if (Flow != nullptr && Flow->IsGameplayLocked())
	{
		return false;
	}

	return GetWorld()->GetTimeSeconds() - LastAttackTime >= CurrentCooldown;
}

// ── 판정 ─────────────────────────────────────────────────

void UTDCombatComponent::PerformHit()
{
	ATDCharacterBase* Owner = Cast<ATDCharacterBase>(GetOwner());

	// 휘두르는 도중에 죽었거나(사망), 취소됐으면(경직) 스윙은 무효다.
	if (Owner == nullptr || Owner->IsDead() || !Attacks.IsValidIndex(PendingAttackIndex))
	{
		return;
	}

	const FTDAttackSpec& Spec = Attacks[PendingAttackIndex];
	PendingAttackIndex = INDEX_NONE;

	for (AActor* Target : GatherTargets(Spec))
	{
		const FTDDamageResult Result = UTDCombatStatics::ApplyDamage(
			Owner, Target, FGameplayTagContainer());

		if (Result.FinalDamage > 0.f)
		{
			// 접촉점 근사치: 공격자에서 가장 가까운 대상 콜리전 표면의 점.
			FVector HitLocation = Target->GetActorLocation();

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

TArray<AActor*> UTDCombatComponent::GatherTargets(const FTDAttackSpec& Spec) const
{
	TArray<AActor*> Targets;

	const ATDCharacterBase* Owner = Cast<ATDCharacterBase>(GetOwner());
	if (Owner == nullptr)
	{
		return Targets;
	}

	// 전방 박스. "마지막 이동 방향" 기준 — 스프라이트 방향(SetDirectionality ← Velocity)과 같은 출처.
	const FQuat BoxRotation = FRotationMatrix::MakeFromX(FacingDirection).ToQuat();
	const FVector Center = Owner->GetActorLocation() + FacingDirection * Spec.HitBoxForwardOffset;

	for (ATDCharacterBase* Enemy : UTDCombatStatics::GatherEnemiesInBox(
		Owner, Center, BoxRotation, Spec.HitBoxExtent, bDrawDebugHitBox))
	{
		Targets.Add(Enemy);
	}
	return Targets;
}

// ── 방송 ─────────────────────────────────────────────────

void UTDCombatComponent::MulticastOnAttack_Implementation(int32 AttackIndex)
{
	OnAttackStarted.Broadcast();
	OnPatternStarted.Broadcast(AttackIndex);
}

void UTDCombatComponent::MulticastOnHit_Implementation(AActor* Target, float Damage, bool bCritical, FVector HitLocation)
{
	OnHit.Broadcast(Target, Damage, bCritical, HitLocation);
}