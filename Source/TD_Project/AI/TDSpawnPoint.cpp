#include "AI/TDSpawnPoint.h"

#include "AI/TDSpawnSubsystem.h"
#include "Character/TDBossCharacter.h"
#include "Character/TDEnemyBase.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ATDSpawnPoint::ATDSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	// 게임에서는 안 보이고 에디터에서만 보이는 배치용 표식.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	UBillboardComponent* Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	Billboard->SetupAttachment(RootComponent);
}

void ATDSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	// 스폰은 서버 권한이다. 클라이언트의 포인트 액터는 표식일 뿐 아무것도 하지 않는다.
	if (!HasAuthority())
	{
		return;
	}

	if (UTDSpawnSubsystem* Subsystem = GetWorld()->GetSubsystem<UTDSpawnSubsystem>())
	{
		Subsystem->RegisterSpawnPoint(this);
	}

	SpawnMonster();
}

void ATDSpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelRespawn();

	if (UTDSpawnSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UTDSpawnSubsystem>() : nullptr)
	{
		Subsystem->UnregisterSpawnPoint(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ATDSpawnPoint::SpawnMonster()
{
	if (!HasAuthority() || CurrentMonster != nullptr)
	{
		return;
	}

	if (MonsterClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: MonsterClass 가 비어 있어 스폰하지 못했다."), *GetName());
		return;
	}

	// 자리가 살짝 겹쳐도 옆으로 밀어내서라도 스폰한다. 리젠이 지형에 막혀 조용히 실패하면 안 된다.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	CurrentMonster = GetWorld()->SpawnActor<ATDEnemyBase>(
		MonsterClass, GetActorLocation(), GetActorRotation(), Params);

	if (CurrentMonster == nullptr)
	{
		return;
	}

	// 행을 지정한 포인트만 덮어쓴다. BeginPlay 의 기본 초기화 뒤에 한 번 더 도는 셈이지만
	// ClearSources 가 있어 중복 적용은 없다.
	if (!MonsterId.IsNone())
	{
		CurrentMonster->InitializeFromDefinition(MonsterId, Level);
	}

	CurrentMonster->OnDeath.AddDynamic(this, &ATDSpawnPoint::HandleMonsterDeath);
}

void ATDSpawnPoint::HandleMonsterDeath()
{
	// 죽은 몬스터와의 연결을 끊는다. 액터 자체는 시체 연출 후 SetLifeSpan 으로 알아서 사라진다.
	CurrentMonster = nullptr;

	// 보스방은 스스로 되살아나지 않는다. 존이 빈 뒤 ResetForZone 이 살린다.
	if (!bAutoRespawn)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		RespawnTimerHandle, this, &ATDSpawnPoint::SpawnMonster, RespawnDelay, false);
}

void ATDSpawnPoint::CancelRespawn()
{
	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
}

void ATDSpawnPoint::ResetForZone()
{
	if (!HasAuthority())
	{
		return;
	}

	// 재스폰이 예약돼 있었으면 기다리지 않고 지금 한다.
	CancelRespawn();

	if (CurrentMonster == nullptr)
	{
		SpawnMonster();
		return;
	}

	// 살아 있다. 이전 파티가 깎아 둔 체력·페이즈·분노가 남으면 안 된다.
	// 일반 몬스터는 되돌릴 전투 상태가 없어 그대로 둔다.
	if (ATDBossCharacter* Boss = Cast<ATDBossCharacter>(CurrentMonster))
	{
		Boss->ResetFight();
	}
}