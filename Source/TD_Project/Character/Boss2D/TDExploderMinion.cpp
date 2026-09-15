#include "Character/Boss2D/TDExploderMinion.h"

#include "AIController.h"
#include "Character/Boss2D/TD2DBossCharacter.h"
#include "Combat/TDCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "Sound/SoundBase.h"

ATDExploderMinion::ATDExploderMinion()
{
	PrimaryActorTick.bCanEverTick = true;
	TeamId = FGenericTeamId(1);
	CorpseLifetime = 0.2f;
}

void ATDExploderMinion::BeginPlay()
{
	Super::BeginPlay();
	RefreshFlipbook();
	if (HasAuthority() && MaxLifetime > 0.f)
	{
		SetLifeSpan(MaxLifetime);
	}
}

void ATDExploderMinion::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ATDExploderMinion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshFlipbook();
	if (!HasAuthority() || IsDead())
	{
		return;
	}

	if (const ATD2DBossCharacter* Boss = GetOwningBoss())
	{
		const FVector Clamped = Boss->ClampLocationToArena(GetActorLocation());
		if (!Clamped.Equals(GetActorLocation(), 0.1f))
		{
			SetActorLocation(Clamped, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

void ATDExploderMinion::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATDExploderMinion, bExploding);
}

ATD2DBossCharacter* ATDExploderMinion::GetOwningBoss() const
{
	return Cast<ATD2DBossCharacter>(GetOwner());
}

void ATDExploderMinion::BeginExplosionFuse()
{
	if (!HasAuthority() || bExploding || IsDead())
	{
		return;
	}

	bExploding = true;
	ForceNetUpdate();
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
		AIC->ClearFocus(EAIFocusPriority::Gameplay);
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	// 경고/퓨즈 대기 없이 접근 즉시 폭발한다.
	Explode();
}

void ATDExploderMinion::Explode()
{
	if (!HasAuthority() || IsDead())
	{
		return;
	}

	const FVector ExplosionLocation = GetActorLocation();
	const FGameplayTagContainer EmptyContext;
	for (ATDCharacterBase* Target : UTDCombatStatics::GatherEnemiesInSphere(this, ExplosionLocation, ExplosionRadius, false))
	{
		UTDCombatStatics::ApplyDamage(this, Target, EmptyContext, ExplosionDamageScale);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	MulticastExplode(ExplosionLocation);
	SetActorHiddenInGame(true);
	SetLifeSpan(FMath::Max(ExplosionDestroyDelay, 0.01f));
}

void ATDExploderMinion::HandleDeath()
{
	bExploding = false;
	Super::HandleDeath();
}

void ATDExploderMinion::MulticastExplode_Implementation(FVector Location)
{
	OnExploded.Broadcast();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (ExplosionVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionVFX, Location);
	}
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, Location);
	}
}

UPaperFlipbook* ATDExploderMinion::PickDirectionalFlipbook(const FTD2DDirectionalFlipbooks& Set) const
{
	UPaperFlipbook* Picked = nullptr;
	if (FMath::Abs(VisualFacing.X) >= FMath::Abs(VisualFacing.Y))
	{
		Picked = VisualFacing.X >= 0.f ? Set.East : Set.West;
	}
	else
	{
		Picked = VisualFacing.Y >= 0.f ? Set.North : Set.South;
	}
	if (Picked != nullptr)
	{
		return Picked;
	}
	return Set.South ? Set.South : (Set.East ? Set.East : (Set.North ? Set.North : Set.West));
}

void ATDExploderMinion::RefreshFlipbook()
{
	if (GetNetMode() == NM_DedicatedServer || GetSpriteComponent() == nullptr || IsHidden())
	{
		return;
	}

	const FVector Velocity2D(GetVelocity().X, GetVelocity().Y, 0.f);
	const bool bMoving = !bExploding && !IsDead() && Velocity2D.SizeSquared() > FMath::Square(5.f);
	if (bMoving)
	{
		VisualFacing = Velocity2D.GetSafeNormal();
	}

	UPaperFlipbook* Desired = PickDirectionalFlipbook(bMoving ? WalkAnimation : IdleAnimation);
	if (Desired == nullptr || Desired == CurrentVisualFlipbook)
	{
		return;
	}

	CurrentVisualFlipbook = Desired;
	GetSpriteComponent()->SetFlipbook(Desired);
	GetSpriteComponent()->SetLooping(true);
	GetSpriteComponent()->PlayFromStart();
}
