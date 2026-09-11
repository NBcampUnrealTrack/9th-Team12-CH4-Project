#include "Character/TDBossCharacter.h"

#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Camera/CameraShakeBase.h"
#include "Combat/TDBossProjectile.h"
#include "Combat/TDCombatComponent.h"
#include "Combat/TDCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ATDBossCharacter::ATDBossCharacter()
{
	// 돌진·잠수 이동은 서버 Tick 에서 굴린다. 클라이언트는 이동 복제로 본다.
	PrimaryActorTick.bCanEverTick = true;

	HitStaggerDuration = 0.f;   // 슈퍼아머 — CanBeStaggered 가 false 라 의미 없지만 명시
	CorpseLifetime = 4.f;       // 사망 연출(가라앉기)을 볼 시간
}

void ATDBossCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATDBossCharacter, Phase);
	DOREPLIFETIME(ATDBossCharacter, bEnraged);
}

void ATDBossCharacter::BeginPlay()
{
	Super::BeginPlay();

	HomeLocation = GetActorLocation();
	PatternReadyTime.Init(0.f, Patterns.Num());

	// 페이즈 판정은 서버가 체력 변경을 직접 듣는다. 체력바와 같은 델리게이트, 구독자만 다르다.
	if (HasAuthority())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
				UTDAttributeSet::GetHealthAttribute()).AddUObject(this, &ATDBossCharacter::HandleHealthChanged);
		}
	}
}

void ATDBossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UTDAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
	}
	ClearFightTimers();
	Super::EndPlay(EndPlayReason);
}

void ATDBossCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}
	if (bDashing)
	{
		TickDash(DeltaSeconds);
	}
	else if (bBurrowed)
	{
		TickBurrow(DeltaSeconds);
	}
}

// ── 조회 ─────────────────────────────────────────────────

bool ATDBossCharacter::IsBusy() const
{
	return CurrentPatternPhase != ETDBossPatternPhase::None || bInEntrance || bInPhaseTransition;
}

bool ATDBossCharacter::IsInvulnerable() const
{
	// 시간 무적(방벽 스킬, 부모)에 보스 고유 무적을 더한다. Super 를 빼면 보스만 방벽이 안 먹는다.
	return Super::IsInvulnerable() || bInEntrance || bInPhaseTransition || bBurrowed;
}

float ATDBossCharacter::GetIncomingDamageMultiplier() const
{
	return CurrentPatternPhase == ETDBossPatternPhase::Recovery ? RecoveryIncomingDamageMultiplier : 1.f;
}

bool ATDBossCharacter::GetPatternSpec(int32 PatternIndex, FTDBossPatternSpec& OutSpec) const
{
	if (!Patterns.IsValidIndex(PatternIndex))
	{
		return false;
	}
	OutSpec = Patterns[PatternIndex];
	return true;
}

FVector ATDBossCharacter::GetFacing() const
{
	const UTDCombatComponent* Combat = GetCombatComponent();
	const FVector Facing = Combat ? Combat->GetFacingDirection() : GetActorForwardVector();
	return Facing.IsNearlyZero() ? GetActorForwardVector().GetSafeNormal2D() : Facing;
}

// ── 전투 시작·리셋 ────────────────────────────────────────

void ATDBossCharacter::BeginFight(ATDCharacterBase* FirstTarget)
{
	if (!HasAuthority() || bFightActive || IsDead())
	{
		return;
	}

	bFightActive = true;
	FightStartTime = GetWorld()->GetTimeSeconds();
	PatternTarget = FirstTarget;

	// 입장 연출: 무적·정지. BP 가 Entrance 이벤트로 포효·카메라 연출을 붙인다.
	bInEntrance = true;
	MulticastBossEvent(ETDBossEvent::Entrance, 0);
	MulticastCameraShake(GetActorLocation(), 2.f);
	GetWorldTimerManager().SetTimer(EntranceTimerHandle, this, &ATDBossCharacter::EndEntrance,
		FMath::Max(EntranceDuration, 0.01f), false);

	if (EnrageAfterSeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(EnrageTimerHandle, this, &ATDBossCharacter::TriggerEnrage,
			EnrageAfterSeconds, false);
	}
}

void ATDBossCharacter::EndEntrance()
{
	bInEntrance = false;
}

void ATDBossCharacter::ResetFight()
{
	// 시체는 리셋하지 않는다. 되살리는 건 스폰포인트 몫이다.
	if (!HasAuthority() || IsDead())
	{
		return;
	}

	ClearFightTimers();
	CancelPattern();
	DestroyMinions();

	bFightActive = false;
	bInEntrance = false;
	bInPhaseTransition = false;
	bEnraged = false;
	PatternTarget = nullptr;
	LastPattern = INDEX_NONE;
	PatternReadyTime.Init(0.f, Patterns.Num());

	if (Phase != 1)
	{
		Phase = 1;
		OnPhaseChanged.Broadcast(Phase);   // 서버는 OnRep 이 안 불린다
	}

	// 풀피. 회복 경로(RestoreHealth)는 "살아 있는 대상"이 전제라 리셋엔 어트리뷰트를 직접 쓴다.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		const float MaxHealth = ASC->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute());
		ASC->SetNumericAttributeBase(UTDAttributeSet::GetHealthAttribute(), MaxHealth);
	}

	SetActorLocation(HomeLocation, false, nullptr, ETeleportType::TeleportPhysics);
	MulticastBossEvent(ETDBossEvent::Reset, 0);
}

void ATDBossCharacter::ClearFightTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TM = World->GetTimerManager();
		TM.ClearTimer(PhaseTimerHandle);
		TM.ClearTimer(StrikeTickHandle);
		TM.ClearTimer(EntranceTimerHandle);
		TM.ClearTimer(EnrageTimerHandle);
		TM.ClearTimer(TransitionTimerHandle);
	}
}

// ── 패턴 선택 ────────────────────────────────────────────

void ATDBossCharacter::SetPatternTarget(ATDCharacterBase* Target)
{
	PatternTarget = Target;
}

ATDCharacterBase* ATDBossCharacter::FindEnemy(bool bFarthest, float MaxRange) const
{
	ATDCharacterBase* Best = nullptr;
	float BestDistSq = bFarthest ? -1.f : FMath::Square(MaxRange);

	for (TActorIterator<ATDCharacterBase> It(GetWorld()); It; ++It)
	{
		ATDCharacterBase* Candidate = *It;
		if (Candidate == nullptr || Candidate == this || Candidate->IsDead() ||
			Candidate->GetGenericTeamId() == GetGenericTeamId())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
		if (DistSq > FMath::Square(MaxRange))
		{
			continue;
		}

		const bool bBetter = bFarthest ? (DistSq > BestDistSq) : (DistSq < BestDistSq);
		if (bBetter)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}
	return Best;
}

int32 ATDBossCharacter::ChoosePattern(float DistanceToTarget) const
{
	const float Now = GetWorld()->GetTimeSeconds();

	TArray<int32> Candidates;
	float TotalWeight = 0.f;

	for (int32 Index = 0; Index < Patterns.Num(); ++Index)
	{
		const FTDBossPatternSpec& Spec = Patterns[Index];
		const bool bReady = !PatternReadyTime.IsValidIndex(Index) || Now >= PatternReadyTime[Index];
		const bool bInRange = DistanceToTarget >= Spec.MinDistance && DistanceToTarget <= Spec.MaxDistance;
		const bool bRepeat = bAvoidRepeatingPattern && Index == LastPattern && Patterns.Num() > 1;

		if (bReady && bInRange && !bRepeat && Spec.Weight > 0.f)
		{
			Candidates.Add(Index);
			TotalWeight += Spec.Weight;
		}
	}

	if (Candidates.Num() == 0)
	{
		return INDEX_NONE;   // BT 가 잠깐 기다렸다 다시 묻는다
	}

	// 가중 랜덤: 룰렛 돌리기.
	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (int32 Index : Candidates)
	{
		Roll -= Patterns[Index].Weight;
		if (Roll <= 0.f)
		{
			return Index;
		}
	}
	return Candidates.Last();
}

// ── 패턴 엔진: 선딜 → 타격 → 후딜 ────────────────────────

bool ATDBossCharacter::StartPattern(int32 PatternIndex)
{
	if (!HasAuthority() || IsDead() || IsBusy() || !Patterns.IsValidIndex(PatternIndex))
	{
		return false;
	}

	const FTDBossPatternSpec& Spec = Patterns[PatternIndex];

	// 대상 쪽을 보고, 그 순간의 위치를 찍어둔다. 이후 대상이 움직여도 판정은 이 자리 기준.
	if (ATDCharacterBase* Target = PatternTarget.Get())
	{
		PatternTargetLocation = Target->GetActorLocation();
		if (UTDCombatComponent* Combat = GetCombatComponent())
		{
			Combat->SetFacingDirection(PatternTargetLocation - GetActorLocation());
		}
	}
	else
	{
		PatternTargetLocation = GetActorLocation() + GetFacing() * Spec.ForwardOffset;
	}

	CurrentPattern = PatternIndex;
	CurrentPatternPhase = ETDBossPatternPhase::Telegraph;

	const float Duration = FMath::Max(ScaledTelegraph(Spec.TelegraphTime), 0.01f);
	const FVector Center = GetStrikeCenter(Spec);

	// 방송 둘: 사건(번호만) + 예고(어디·어느 방향·얼마나). BP 는 후자로 바닥 표시를 그린다.
	MulticastBossEvent(ETDBossEvent::PatternTelegraph, PatternIndex);
	MulticastPatternTelegraph(PatternIndex, Center, GetFacing(), Duration);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugHits)
	{
		if (Spec.Shape == ETDBossHitShape::Box)
		{
			DrawDebugBox(GetWorld(), Center, Spec.BoxExtent,
				FRotationMatrix::MakeFromX(GetFacing()).ToQuat(), FColor::Yellow, false, Duration);
		}
		else
		{
			DrawDebugSphere(GetWorld(), Center, Spec.SphereRadius, 24, FColor::Yellow, false, Duration);
		}
	}
#endif

	OnTelegraphBegin(Spec);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATDBossCharacter::EnterStrike, Duration, false);
	return true;
}

void ATDBossCharacter::EnterStrike()
{
	if (!Patterns.IsValidIndex(CurrentPattern))
	{
		CancelPattern();
		return;
	}
	const FTDBossPatternSpec& Spec = Patterns[CurrentPattern];

	CurrentPatternPhase = ETDBossPatternPhase::Strike;
	HitThisStrike.Empty();
	MulticastBossEvent(ETDBossEvent::PatternStrike, CurrentPattern);

	if (Spec.ShakeScale > 0.f)
	{
		MulticastCameraShake(GetStrikeCenter(Spec), Spec.ShakeScale);
	}

	OnMotionBegin(Spec);

	// 투사체는 탄이 스스로 판정한다. 나머지는 모양 판정 — 한 번, 또는 StrikeTime 동안 간격 반복.
	if (Spec.Motion != ETDBossMotion::Projectile)
	{
		DoStrikeHit();
		if (Spec.StrikeInterval > 0.f && Spec.StrikeTime > Spec.StrikeInterval)
		{
			GetWorldTimerManager().SetTimer(StrikeTickHandle, this, &ATDBossCharacter::DoStrikeHit,
				Spec.StrikeInterval, true);
		}
	}

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATDBossCharacter::EnterRecovery,
		FMath::Max(Spec.StrikeTime, 0.01f), false);
}

FVector ATDBossCharacter::GetStrikeCenter(const FTDBossPatternSpec& Spec) const
{
	if (Spec.Motion == ETDBossMotion::Burrow)
	{
		// 찍어둔 대상 자리. 높이는 보스 것 — 땅속에서 그 자리로 솟는다.
		return FVector(PatternTargetLocation.X, PatternTargetLocation.Y, GetActorLocation().Z);
	}
	return GetActorLocation() + GetFacing() * Spec.ForwardOffset;
}

void ATDBossCharacter::DoStrikeHit()
{
	if (!Patterns.IsValidIndex(CurrentPattern) || IsDead())
	{
		return;
	}
	const FTDBossPatternSpec& Spec = Patterns[CurrentPattern];
	const FVector Center = GetStrikeCenter(Spec);

	TArray<ATDCharacterBase*> Enemies;
	if (Spec.Shape == ETDBossHitShape::Box)
	{
		const FQuat Rot = FRotationMatrix::MakeFromX(GetFacing()).ToQuat();
		Enemies = UTDCombatStatics::GatherEnemiesInBox(this, Center, Rot, Spec.BoxExtent, bDrawDebugHits);
	}
	else
	{
		Enemies = UTDCombatStatics::GatherEnemiesInSphere(this, Center, Spec.SphereRadius, bDrawDebugHits);
	}

	UTDCombatComponent* Combat = GetCombatComponent();

	for (ATDCharacterBase* Enemy : Enemies)
	{
		if (HitThisStrike.Contains(Enemy))
		{
			continue;   // 반복 판정이어도 한 대상 1회
		}
		const FTDDamageResult Result = UTDCombatStatics::ApplyDamage(
			this, Enemy, FGameplayTagContainer(), Spec.DamageScale);
		if (Result.FinalDamage > 0.f)
		{
			HitThisStrike.Add(Enemy);

			// 평타·스킬과 같은 OnHit 접점으로. 팝업·히트 VFX 가 보스용 델리게이트를 따로 안 배워도 된다.
			if (Combat != nullptr)
			{
				Combat->NotifyHit(Enemy, Result.FinalDamage, Result.bCritical, Enemy->GetActorLocation());
			}
		}
	}
}

void ATDBossCharacter::EnterRecovery()
{
	GetWorldTimerManager().ClearTimer(StrikeTickHandle);

	if (!Patterns.IsValidIndex(CurrentPattern))
	{
		CancelPattern();
		return;
	}
	const FTDBossPatternSpec& Spec = Patterns[CurrentPattern];

	OnMotionEnd(Spec);
	CurrentPatternPhase = ETDBossPatternPhase::Recovery;
	MulticastBossEvent(ETDBossEvent::PatternRecovery, CurrentPattern);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATDBossCharacter::FinishPattern,
		FMath::Max(ScaledRecovery(Spec.RecoveryTime), 0.01f), false);
}

void ATDBossCharacter::FinishPattern()
{
	const int32 Finished = CurrentPattern;
	if (Patterns.IsValidIndex(Finished) && PatternReadyTime.IsValidIndex(Finished))
	{
		PatternReadyTime[Finished] = GetWorld()->GetTimeSeconds() + Patterns[Finished].Cooldown;
	}

	LastPattern = Finished;
	CurrentPattern = INDEX_NONE;
	CurrentPatternPhase = ETDBossPatternPhase::None;
	MulticastBossEvent(ETDBossEvent::PatternEnd, Finished);
	OnPatternFinished.Broadcast(Finished);
}

void ATDBossCharacter::CancelPattern()
{
	// 페이즈 전환·사망·리셋이 패턴을 끊을 때. 쿨은 안 건다. 이동 상태도 원복.
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
	GetWorldTimerManager().ClearTimer(StrikeTickHandle);
	EndDash(false);
	EndBurrow();

	if (CurrentPatternPhase == ETDBossPatternPhase::None)
	{
		return;
	}

	const int32 Cancelled = CurrentPattern;
	CurrentPattern = INDEX_NONE;
	CurrentPatternPhase = ETDBossPatternPhase::None;
	MulticastBossEvent(ETDBossEvent::PatternEnd, Cancelled);
	OnPatternFinished.Broadcast(Cancelled);
}

float ATDBossCharacter::ScaledTelegraph(float Base) const
{
	float Scale = 1.f;
	if (Phase >= 2) Scale *= Phase2TelegraphScale;
	if (bEnraged)   Scale *= EnrageTelegraphScale;
	return Base * Scale;
}

float ATDBossCharacter::ScaledRecovery(float Base) const
{
	return Base * (Phase >= 2 ? Phase2RecoveryScale : 1.f);
}

// ── 이동 훅 ──────────────────────────────────────────────

void ATDBossCharacter::OnTelegraphBegin(const FTDBossPatternSpec& Spec)
{
	if (Spec.Motion == ETDBossMotion::Burrow)
	{
		StartBurrow(Spec);
	}
}

void ATDBossCharacter::OnMotionBegin(const FTDBossPatternSpec& Spec)
{
	switch (Spec.Motion)
	{
	case ETDBossMotion::Dash:       StartDash();            break;
	case ETDBossMotion::Burrow:     EndBurrow();            break;   // 출현
	case ETDBossMotion::Projectile: FireProjectiles(Spec);  break;
	default: break;
	}
}

void ATDBossCharacter::OnMotionEnd(const FTDBossPatternSpec& Spec)
{
	if (Spec.Motion == ETDBossMotion::Dash)
	{
		EndDash(false);
	}
}

// ── 돌진 ─────────────────────────────────────────────────

void ATDBossCharacter::StartDash()
{
	DashDirection = GetFacing();
	bDashing = true;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
}

void ATDBossCharacter::TickDash(float DeltaSeconds)
{
	FHitResult Hit;
	AddActorWorldOffset(DashDirection * DashSpeed * DeltaSeconds, true, &Hit);

	// 벽(또는 큰 장애물)에 박으면 거기서 끝. 판정 반복은 StrikeTime 이 끝날 때까지 계속되지만
	// 몸이 안 움직이니 같은 자리만 때린다 — 이미 맞은 대상은 1회 규칙으로 걸러진다.
	if (Hit.bBlockingHit && !Cast<ATDCharacterBase>(Hit.GetActor()))
	{
		EndDash(true);
	}
}

void ATDBossCharacter::EndDash(bool bHitWall)
{
	if (!bDashing)
	{
		return;
	}
	bDashing = false;
	if (bHitWall)
	{
		MulticastCameraShake(GetActorLocation(), 1.5f);
	}
}

// ── 잠수 ─────────────────────────────────────────────────

void ATDBossCharacter::StartBurrow(const FTDBossPatternSpec& Spec)
{
	bBurrowed = true;
	BurrowFrom = GetActorLocation();
	BurrowTo = GetStrikeCenter(Spec);   // 찍어둔 대상 자리(보스 높이)
	BurrowElapsed = 0.f;
	BurrowDuration = FMath::Max(ScaledTelegraph(Spec.TelegraphTime), 0.01f);

	// 땅속: 안 보이고, 안 부딪히고, 안 맞는다(IsInvulnerable). 숨김은 복제되는 속성이라 클라도 따라온다.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetActorHiddenInGame(true);
}

void ATDBossCharacter::TickBurrow(float DeltaSeconds)
{
	BurrowElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(BurrowElapsed / BurrowDuration, 0.f, 1.f);
	SetActorLocation(FMath::Lerp(BurrowFrom, BurrowTo, Alpha), false, nullptr, ETeleportType::TeleportPhysics);
}

void ATDBossCharacter::EndBurrow()
{
	if (!bBurrowed)
	{
		return;
	}
	bBurrowed = false;

	// 출현: 정확히 목표 자리에서. 취소로 끊길 때도 여기로 오므로 현재 위치가 아닌 목표를 쓴다.
	SetActorLocation(BurrowTo, false, nullptr, ETeleportType::TeleportPhysics);
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	SetActorHiddenInGame(false);
}

// ── 물대포 ───────────────────────────────────────────────

void ATDBossCharacter::FireProjectiles(const FTDBossPatternSpec& Spec)
{
	if (ProjectileClass == nullptr)
	{
		return;
	}

	const FVector Facing = GetFacing();
	const float Radius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
	const FVector Muzzle = GetActorLocation() + Facing * (Radius + ProjectileMuzzleForward)
		+ FVector(0.f, 0.f, ProjectileMuzzleHeight);

	// 페이즈 2 는 부채꼴. 가운데 발이 정면, 나머지는 좌우로 SpreadAngle 씩.
	const int32 Count = Phase >= 2 ? FMath::Max(1, ProjectileCountPhase2) : 1;
	const float StartAngle = -ProjectileSpreadAngle * (Count - 1) * 0.5f;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector Direction = Facing.RotateAngleAxis(StartAngle + ProjectileSpreadAngle * Index, FVector::UpVector);
		ATDBossProjectile* Projectile = GetWorld()->SpawnActor<ATDBossProjectile>(
			ProjectileClass, Muzzle, Direction.Rotation(), Params);
		if (Projectile != nullptr)
		{
			Projectile->Init(this, Direction, Spec.DamageScale, ProjectileSpeed);
		}
	}
}

// ── 페이즈·분노 ───────────────────────────────────────────

void ATDBossCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (!HasAuthority() || Phase >= 2 || IsDead())
	{
		return;
	}
	if (Data.NewValue <= 0.f)
	{
		return;   // 죽는 타격. 페이즈 전환은 HandleDeath 에 양보한다.
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	const float MaxHealth = ASC->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute());
	if (MaxHealth <= 0.f)
	{
		return;
	}
	if (Data.NewValue / MaxHealth <= Phase2HealthRatio)
	{
		EnterPhase2();
	}
}

void ATDBossCharacter::EnterPhase2()
{
	if (!HasAuthority() || Phase >= 2 || IsDead())
	{
		return;
	}

	CancelPattern();   // 진행 중이던 패턴은 끊고 전환 연출로

	Phase = 2;
	OnPhaseChanged.Broadcast(Phase);   // 서버는 OnRep 이 안 불린다

	bInPhaseTransition = true;
	MulticastBossEvent(ETDBossEvent::Phase2, 0);
	MulticastCameraShake(GetActorLocation(), 3.f);

	if (bSummonOnPhase2)
	{
		SummonMinions();
	}

	GetWorldTimerManager().SetTimer(TransitionTimerHandle, this, &ATDBossCharacter::EndPhase2Transition,
		FMath::Max(Phase2TransitionDuration, 0.01f), false);
}

void ATDBossCharacter::EndPhase2Transition()
{
	bInPhaseTransition = false;
}

void ATDBossCharacter::OnRep_Phase()
{
	OnPhaseChanged.Broadcast(Phase);
}

void ATDBossCharacter::TriggerEnrage()
{
	if (!HasAuthority() || bEnraged || IsDead())
	{
		return;
	}
	bEnraged = true;
	MulticastBossEvent(ETDBossEvent::Enrage, 0);
	MulticastCameraShake(GetActorLocation(), 2.f);
}

// ── 소환 ─────────────────────────────────────────────────

void ATDBossCharacter::SummonMinions()
{
	if (!HasAuthority() || MinionClass == nullptr || MinionCount <= 0)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// 보스 발 근처 높이. 캡슐 중심에서 스폰하면 큰 보스일수록 쫄이 하늘에서 떨어진다.
	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;

	int32 Spawned = 0;
	for (int32 Index = 0; Index < MinionCount; ++Index)
	{
		const float Angle = (2.f * PI / MinionCount) * Index;
		FVector SpawnLocation = GetActorLocation()
			+ FVector(FMath::Cos(Angle) * MinionSpawnRadius, FMath::Sin(Angle) * MinionSpawnRadius, 0.f);
		SpawnLocation.Z -= HalfHeight - 100.f;

		ATDEnemyBase* Minion = GetWorld()->SpawnActor<ATDEnemyBase>(
			MinionClass, SpawnLocation, GetActorRotation(), Params);
		if (Minion == nullptr)
		{
			continue;
		}
		if (!MinionId.IsNone())
		{
			Minion->InitializeFromDefinition(MinionId, GetLevel());
		}
		Minions.Add(Minion);
		++Spawned;
	}

	if (Spawned > 0)
	{
		MulticastBossEvent(ETDBossEvent::Summon, Spawned);
	}
}

void ATDBossCharacter::DestroyMinions()
{
	for (const TWeakObjectPtr<ATDEnemyBase>& Minion : Minions)
	{
		if (Minion.IsValid())
		{
			Minion->Destroy();
		}
	}
	Minions.Empty();
}

// ── 사망 ─────────────────────────────────────────────────

void ATDBossCharacter::HandleDeath()
{
	ClearFightTimers();
	CancelPattern();   // 돌진·잠수 상태도 여기서 원복된다
	DestroyMinions();
	bFightActive = false;
	bInEntrance = false;
	bInPhaseTransition = false;

	MulticastBossEvent(ETDBossEvent::Death, 0);
	MulticastCameraShake(GetActorLocation(), 3.f);

	Super::HandleDeath();   // 콜리전 off·이동 정지·시체 수명
}

// ── 방송 ─────────────────────────────────────────────────

void ATDBossCharacter::MulticastBossEvent_Implementation(ETDBossEvent Event, int32 Param)
{
	OnBossEvent.Broadcast(Event, Param);
}

void ATDBossCharacter::MulticastPatternTelegraph_Implementation(int32 PatternIndex, FVector Center, FVector Direction, float Duration)
{
	OnPatternTelegraph.Broadcast(PatternIndex, Center, Direction, Duration);
}

void ATDBossCharacter::MulticastCameraShake_Implementation(FVector Epicenter, float Scale)
{
	PlayShakeLocally(Epicenter, Scale);
}

void ATDBossCharacter::PlayShakeLocally(const FVector& Epicenter, float Scale) const
{
	if (CameraShakeClass == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC == nullptr || !PC->IsLocalController())
		{
			continue;
		}

		float Falloff = 1.f;
		if (const APawn* ViewPawn = PC->GetPawn())
		{
			const float Distance = FVector::Dist(ViewPawn->GetActorLocation(), Epicenter);
			const float Range = FMath::Max(ShakeOuterRadius - ShakeInnerRadius, 1.f);
			Falloff = 1.f - FMath::Clamp((Distance - ShakeInnerRadius) / Range, 0.f, 1.f);
		}

		if (Falloff > 0.f)
		{
			PC->ClientStartCameraShake(CameraShakeClass, Scale * Falloff);
		}
	}
}