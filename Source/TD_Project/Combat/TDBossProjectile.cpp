#include "Combat/TDBossProjectile.h"

#include "Character/TDCharacterBase.h"
#include "Combat/TDCombatComponent.h"
#include "Combat/TDCombatStatics.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

ATDBossProjectile::ATDBossProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// 서버가 만들고 클라이언트는 이동을 받아 그린다.
	bReplicates = true;
	SetReplicatingMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(40.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);        // 캐릭터는 겹침으로 판정
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);   // 벽에 막히면 소멸
	Collision->SetGenerateOverlapEvents(true);
	RootComponent = Collision;

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 1200.f;
	Movement->MaxSpeed = 1200.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->ProjectileGravityScale = 0.f;   // 2.5D — 직선으로 날아간다
	Movement->bShouldBounce = false;
}

void ATDBossProjectile::Init(ATDCharacterBase* InShooter, const FVector& Direction, float InDamageScale, float Speed, float InKnockback)
{
	Shooter = InShooter;
	DamageScale = InDamageScale;
	Knockback = InKnockback;

	if (InShooter != nullptr)
	{
		Collision->IgnoreActorWhenMoving(InShooter, true);   // 쏜 놈 몸에서 시작해도 안 맞는다
	}

	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;
	// 위아래 기울기를 살린다 — 보스가 입 높이에서 대상 몸통으로 내리꽂도록 조준해 준다.
	Movement->Velocity = Direction.GetSafeNormal() * Speed;
}

void ATDBossProjectile::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(LifeSeconds);

	if (HasAuthority())
	{
		Collision->OnComponentBeginOverlap.AddDynamic(this, &ATDBossProjectile::HandleOverlap);
		Movement->OnProjectileStop.AddDynamic(this, &ATDBossProjectile::HandleStop);
	}
}

void ATDBossProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !Shooter.IsValid())
	{
		return;
	}

	ATDCharacterBase* Target = Cast<ATDCharacterBase>(OtherActor);
	if (Target == nullptr || Target == Shooter.Get() || Target->IsDead() || HitActors.Contains(Target))
	{
		return;
	}
	if (Target->GetGenericTeamId() == Shooter->GetGenericTeamId())
	{
		return;   // 쫄은 안 맞는다
	}

	HitActors.Add(Target);

	const FTDDamageResult Result = UTDCombatStatics::ApplyDamage(
		Shooter.Get(), Target, FGameplayTagContainer(), DamageScale);

	if (Result.FinalDamage > 0.f)
	{
		if (UTDCombatComponent* Combat = Shooter->GetCombatComponent())
		{
			Combat->NotifyHit(Target, Result.FinalDamage, Result.bCritical, GetActorLocation());
		}
		
		if (Knockback > 0.f)
		{
			const FVector Dir = Movement->Velocity.GetSafeNormal2D();
			Target->LaunchCharacter(Dir * Knockback + FVector(0.f, 0.f, Knockback * 0.3f), true, true);
		}
	}

	// 명중 이펙트는 파괴보다 먼저 방송한다(Reliable 이라 채널이 닫히기 전에 나간다).
	MulticastImpact(GetActorLocation());
	if (!bPierce)
	{
		Destroy();
	}
}

void ATDBossProjectile::HandleStop(const FHitResult& ImpactResult)
{
	if (HasAuthority())
	{
		MulticastImpact(ImpactResult.bBlockingHit ? FVector(ImpactResult.ImpactPoint) : GetActorLocation());
		Destroy();   // 벽
	}
}

void ATDBossProjectile::MulticastImpact_Implementation(FVector Location)
{
	if (UNiagaraSystem* System = ImpactVFX.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Location, FRotator::ZeroRotator,
			FVector(ImpactVFXScale), /*bAutoDestroy*/ true, /*bAutoActivate*/ true);
	}
	OnImpact(Location);
}