#include "Combat/TDBossProjectile.h"

#include "Character/TDCharacterBase.h"
#include "Combat/TDCombatComponent.h"
#include "Combat/TDCombatStatics.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

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

void ATDBossProjectile::Init(ATDCharacterBase* InShooter, const FVector& Direction, float InDamageScale, float Speed)
{
	Shooter = InShooter;
	DamageScale = InDamageScale;

	if (InShooter != nullptr)
	{
		Collision->IgnoreActorWhenMoving(InShooter, true);   // 쏜 놈 몸에서 시작해도 안 맞는다
	}

	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;
	Movement->Velocity = Direction.GetSafeNormal2D() * Speed;
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
	}

	if (!bPierce)
	{
		Destroy();
	}
}

void ATDBossProjectile::HandleStop(const FHitResult& ImpactResult)
{
	if (HasAuthority())
	{
		Destroy();   // 벽
	}
}