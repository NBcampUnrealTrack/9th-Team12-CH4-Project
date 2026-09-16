#include "AI/TDSpawnPoint.h"

#include "AI/TDSpawnSubsystem.h"
#include "Character/TDBossCharacter.h"
#include "Character/TDEnemyBase.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Player/TDPlayerState.h"
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

	// 첫 검사는 바로 한다. 인스턴싱을 끈 자리는 이 한 번으로 예전처럼 몬스터가 선다.
	UpdateSlots();

	GetWorldTimerManager().SetTimer(
		UpdateTimerHandle, this, &ATDSpawnPoint::UpdateSlots, FMath::Max(0.1f, UpdateInterval), true);
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
	if (!HasAuthority())
	{
		return;
	}

	// 그룹 없는 공용 자리. 인스턴싱을 끈 포인트가 쓰는 길이다.
	FTDSpawnSlot& Slot = Slots.FindOrAdd(FGuid());
	if (Slot.Monster == nullptr)
	{
		SpawnForGroup(FGuid(), Slot);
	}
}

void ATDSpawnPoint::SpawnForGroup(const FGuid& GroupId, FTDSpawnSlot& Slot)
{
	if (MonsterClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: MonsterClass 가 비어 있어 스폰하지 못했다."), *GetName());
		return;
	}

	// 자리가 살짝 겹쳐도 옆으로 밀어내서라도 스폰한다. 리젠이 지형에 막혀 조용히 실패하면 안 된다.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ATDEnemyBase* Monster = GetWorld()->SpawnActor<ATDEnemyBase>(
		MonsterClass, GetActorLocation(), GetActorRotation(), Params);

	if (Monster == nullptr)
	{
		return;
	}

	// **스폰 직후에 넣는다.** 이 값이 비어 있는 동안에는 모두에게 보이는 공용 몬스터라,
	// 한 프레임이라도 늦으면 남의 화면에 잠깐 나타났다 사라진다.
	Monster->SetOwnerGroupId(GroupId);

	// 행을 지정한 포인트만 덮어쓴다. BeginPlay 의 기본 초기화 뒤에 한 번 더 도는 셈이지만
	// ClearSources 가 있어 중복 적용은 없다.
	if (!MonsterId.IsNone())
	{
		Monster->InitializeFromDefinition(MonsterId, Level);
	}

	Slot.Monster = Monster;
}

TSet<FGuid> ATDSpawnPoint::GatherActiveGroups() const
{
	TSet<FGuid> Groups;

	// 인스턴싱을 끈 자리는 언제나 공용 몬스터 한 마리다(예전 동작).
	if (!bInstancePerGroup)
	{
		Groups.Add(FGuid());
		return Groups;
	}

	const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (State == nullptr)
	{
		return Groups;
	}

	const float RadiusSq = FMath::Square(ActivationRadius);

	for (APlayerState* PlayerState : State->PlayerArray)
	{
		const ATDPlayerState* TDState = Cast<ATDPlayerState>(PlayerState);
		if (TDState == nullptr)
		{
			continue;
		}

		// 캐릭터를 아직 고르지 않았거나 죽어서 Pawn 이 없는 사람은 세지 않는다.
		const APawn* Pawn = TDState->GetPawn();
		if (Pawn == nullptr)
		{
			continue;
		}

		if (FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) > RadiusSq)
		{
			continue;
		}

		Groups.Add(TDState->GetInstanceGroupId());
	}

	return Groups;
}

void ATDSpawnPoint::RefreshMoveIgnores(ATDEnemyBase* Monster, const FGuid& GroupId)
{
	// 공용 몬스터(보스·배치형)는 모두의 것이라 통과시킬 이유가 없다.
	if (Monster == nullptr || !GroupId.IsValid())
	{
		return;
	}

	const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (State == nullptr)
	{
		return;
	}

	for (APlayerState* PlayerState : State->PlayerArray)
	{
		const ATDPlayerState* TDState = Cast<ATDPlayerState>(PlayerState);
		if (TDState == nullptr)
		{
			continue;
		}

		APawn* Pawn = TDState->GetPawn();
		if (Pawn == nullptr)
		{
			continue;
		}

		// 내 그룹이면 그대로 막고(보스도 여기 해당한다), 남이면 서로 통과시킨다.
		const bool bSameGroup = TDState->GetInstanceGroupId() == GroupId;
		Monster->SetMoveIgnoredByPawn(Pawn, !bSameGroup);
	}
}

void ATDSpawnPoint::UpdateSlots()
{
	if (!HasAuthority() || GetWorld() == nullptr)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const TSet<FGuid> ActiveGroups = GatherActiveGroups();

	// 새로 온 그룹의 자리를 만들고, 지금 있는 그룹은 본 시각을 갱신한다.
	for (const FGuid& GroupId : ActiveGroups)
	{
		Slots.FindOrAdd(GroupId).LastSeenTime = Now;
	}

	for (TMap<FGuid, FTDSpawnSlot>::TIterator It(Slots); It; ++It)
	{
		FTDSpawnSlot& Slot = It.Value();
		const bool bActive = ActiveGroups.Contains(It.Key());

		// 죽은 것을 방금 알았다. 지금부터 재스폰 시간을 센다.
		if (Slot.Monster != nullptr && Slot.Monster->IsDead())
		{
			Slot.Monster = nullptr;
			Slot.RespawnTime = Now + RespawnDelay;
		}

		// 남의 몬스터가 길을 막지 않게 통과 쌍을 맞춘다. 파티 가입·탈퇴로 그룹이 바뀌어도
		// 여기서 다시 계산하므로 따로 처리할 곳이 없다.
		if (Slot.Monster != nullptr)
		{
			RefreshMoveIgnores(Slot.Monster, It.Key());
		}

		// 아무도 없는 자리는 유예가 지나면 정리한다. 몬스터가 남아 있으면 함께 지운다 —
		// 그대로 두면 아무도 볼 수 없는 몬스터가 서버에서 계속 생각하게 된다.
		if (!bActive && Now - Slot.LastSeenTime >= GroupExitGrace)
		{
			if (Slot.Monster != nullptr)
			{
				Slot.Monster->Destroy();
			}

			It.RemoveCurrent();
			continue;
		}

		if (!bActive || Slot.Monster != nullptr)
		{
			continue;
		}

		// 보스방은 스스로 되살아나지 않는다. 존이 빈 뒤 ResetForZone 이 살린다.
		if (!bAutoRespawn || Now < Slot.RespawnTime)
		{
			continue;
		}

		SpawnForGroup(It.Key(), Slot);
	}
}

void ATDSpawnPoint::CancelRespawn()
{
	GetWorldTimerManager().ClearTimer(UpdateTimerHandle);
}

void ATDSpawnPoint::ResetForZone()
{
	if (!HasAuthority())
	{
		return;
	}

	for (TPair<FGuid, FTDSpawnSlot>& Pair : Slots)
	{
		FTDSpawnSlot& Slot = Pair.Value;

		// 기다리던 재스폰이 있었으면 지금 한다.
		if (Slot.Monster == nullptr || Slot.Monster->IsDead())
		{
			Slot.Monster = nullptr;
			Slot.RespawnTime = 0.f;
			SpawnForGroup(Pair.Key, Slot);
			continue;
		}

		// 살아 있다. 이전 파티가 깎아 둔 체력·페이즈·분노가 남으면 안 된다.
		// 일반 몬스터는 되돌릴 전투 상태가 없어 그대로 둔다.
		if (ATDBossCharacter* Boss = Cast<ATDBossCharacter>(Slot.Monster))
		{
			Boss->ResetFight();
		}
	}

	// 보스방처럼 인스턴싱을 끈 자리인데 아직 한 번도 안 만들어졌으면 여기서 세운다.
	if (Slots.IsEmpty())
	{
		SpawnMonster();
	}
}
