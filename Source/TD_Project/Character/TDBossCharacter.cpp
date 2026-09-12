#include "Character/TDBossCharacter.h"

#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "AIController.h"
#include "Camera/CameraShakeBase.h"
#include "Combat/TDBossProjectile.h"
#include "Combat/TDCombatComponent.h"
#include "Combat/TDCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDMonsterRow.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/TDGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Stats/TDStatComponent.h"
#include "TimerManager.h"

ATDBossCharacter::ATDBossCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	HitStaggerDuration = 0.f;
	CorpseLifetime = 4.f;
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
	ClearTelegraphVFX();
	if (HasAuthority())
	{
		RegisterActiveBoss(false);
	}
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
	else if (bReturning)
	{
		TickReturn();
	}
}

// ── 조회 ─────────────────────────────────────────────────

bool ATDBossCharacter::IsBusy() const
{
	return CurrentPatternPhase != ETDBossPatternPhase::None || bInEntrance || bInPhaseTransition || bReturning;
}

bool ATDBossCharacter::IsInvulnerable() const
{
	// 시간 무적(방벽 스킬, 부모)에 보스 고유 무적을 더한다. Super 를 빼면 보스만 방벽이 안 먹는다.
	return Super::IsInvulnerable() || bInEntrance || bInPhaseTransition || bBurrowed || bReturning;
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

FText ATDBossCharacter::GetDisplayName() const
{
	if (MonsterTable != nullptr && !MonsterId.IsNone())
	{
		if (const FTDMonsterRow* Row = MonsterTable->FindRow<FTDMonsterRow>(MonsterId, TEXT("BossDisplayName")))
		{
			return Row->DisplayName;
		}
	}
	return FText::FromName(MonsterId);
}

FVector ATDBossCharacter::GetFacing() const
{
	const UTDCombatComponent* Combat = GetCombatComponent();
	const FVector Facing = Combat ? Combat->GetFacingDirection() : GetActorForwardVector();
	return Facing.IsNearlyZero() ? GetActorForwardVector().GetSafeNormal2D() : Facing;
}

// ── 전투 시작·리셋·귀환 ───────────────────────────────────

void ATDBossCharacter::BeginFight(ATDCharacterBase* FirstTarget)
{
	if (!HasAuthority() || bFightActive || bReturning || IsDead())
	{
		return;
	}

	bFightActive = true;
	FightStartTime = GetWorld()->GetTimeSeconds();
	PatternTarget = FirstTarget;
	RegisterActiveBoss(true);

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

void ATDBossCharacter::ResetFight(bool bInstant)
{
	if (!HasAuthority() || IsDead() || bReturning)
	{
		return;
	}

	// 전투 상태는 즉시 끊는다. 풀피·페이즈 복구는 집에 도착했을 때(FinishReturnHome).
	ClearFightTimers();
	CancelPattern();
	DestroyMinions();
	bFightActive = false;
	bInEntrance = false;
	bInPhaseTransition = false;
	PatternTarget = nullptr;
	RegisterActiveBoss(false);

	const bool bAlreadyHome = FVector::Dist2D(GetActorLocation(), HomeLocation) <= ReturnArriveDistance;
	if (bInstant || bAlreadyHome)
	{
		SetActorLocation(HomeLocation, false, nullptr, ETeleportType::TeleportPhysics);
		FinishReturnHome();
		return;
	}
	StartReturnHome();
}

void ATDBossCharacter::StartReturnHome()
{
	// 귀환 중: 무적, 판단 정지(IsBusy), 공격 안 함. 플레이어가 끌고 나가 봤자 얻을 게 없게.
	bReturning = true;
	MulticastBossEvent(ETDBossEvent::Reset, 0);

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
		AIC->MoveToLocation(HomeLocation, 50.f);
	}

	// 길이 막혀 못 오면 순간이동으로 마무리. 리시가 영원히 안 풀리는 것보다 낫다.
	GetWorldTimerManager().SetTimer(ReturnTimeoutHandle, [this]()
	{
		if (bReturning)
		{
			SetActorLocation(HomeLocation, false, nullptr, ETeleportType::TeleportPhysics);
			FinishReturnHome();
		}
	}, ReturnTimeout, false);
}

void ATDBossCharacter::TickReturn()
{
	if (FVector::Dist2D(GetActorLocation(), HomeLocation) <= ReturnArriveDistance)
	{
		FinishReturnHome();
	}
}

void ATDBossCharacter::FinishReturnHome()
{
	GetWorldTimerManager().ClearTimer(ReturnTimeoutHandle);
	bReturning = false;

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
	}

	bEnraged = false;
	LastPattern = INDEX_NONE;
	PatternReadyTime.Init(0.f, Patterns.Num());
	ClearBossBuff();

	if (Phase != 1)
	{
		Phase = 1;
		OnPhaseChanged.Broadcast(Phase);   // 서버는 OnRep 이 안 불린다
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		const float MaxHealth = ASC->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute());
		ASC->SetNumericAttributeBase(UTDAttributeSet::GetHealthAttribute(), MaxHealth);
	}
}

void ATDBossCharacter::RegisterActiveBoss(bool bActive)
{
	ATDGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ATDGameState>() : nullptr;
	if (GameState == nullptr)
	{
		return;
	}
	if (bActive)
	{
		GameState->SetActiveBoss(this);
	}
	else if (GameState->GetActiveBoss() == this)
	{
		GameState->SetActiveBoss(nullptr);   // 다른 보스가 등록돼 있으면 건드리지 않는다
	}
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
		TM.ClearTimer(ReturnTimeoutHandle);
	}
}

// ── 강화 (스탯 소스) ──────────────────────────────────────

void ATDBossCharacter::ApplyBossBuff()
{
	UTDStatComponent* Stats = GetStatComponent();
	if (Stats == nullptr)
	{
		return;
	}

	// 페이즈 2 와 분노가 합산된다. ReplaceSource 라 몇 번 불러도 소스는 하나.
	const float Bonus = (Phase >= 2 ? Phase2DamageBonus : 0.f) + (bEnraged ? EnrageDamageBonus : 0.f);

	TArray<FTDStatModifier> Modifiers;
	Modifiers.Add(FTDStatModifier(TDTags::Stat_Offense_Damage_Physical, ETDModOp::Increased, Bonus));
	Modifiers.Add(FTDStatModifier(TDTags::Stat_Offense_Damage_Magical, ETDModOp::Increased, Bonus));
	BossBuffHandle = Stats->ReplaceSource(BossBuffHandle, TDTags::Source_Boss, MoveTemp(Modifiers));
}

void ATDBossCharacter::ClearBossBuff()
{
	if (UTDStatComponent* Stats = GetStatComponent())
	{
		if (BossBuffHandle.IsValid())
		{
			Stats->RemoveSource(BossBuffHandle);
		}
	}
	BossBuffHandle.Invalidate();
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
		return INDEX_NONE;
	}

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

	OnMotionBegin(Spec);   // 잠수는 여기서 출현하므로, 중심 계산은 그 뒤에

	const FVector Center = GetStrikeCenter(Spec);
	MulticastPatternStrike(CurrentPattern, Center, GetFacing());

	if (Spec.ShakeScale > 0.f)
	{
		MulticastCameraShake(Center, Spec.ShakeScale);
	}

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
			continue;
		}
		const FTDDamageResult Result = UTDCombatStatics::ApplyDamage(
			this, Enemy, FGameplayTagContainer(), Spec.DamageScale);
		if (Result.FinalDamage > 0.f)
		{
			HitThisStrike.Add(Enemy);
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
	case ETDBossMotion::Burrow:     EndBurrow();            break;
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
	BurrowTo = GetStrikeCenter(Spec);
	BurrowElapsed = 0.f;
	BurrowDuration = FMath::Max(ScaledTelegraph(Spec.TelegraphTime), 0.01f);

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
	if (!HasAuthority() || Phase >= 2 || IsDead() || Data.NewValue <= 0.f)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	const float MaxHealth = ASC->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute());
	if (MaxHealth > 0.f && Data.NewValue / MaxHealth <= Phase2HealthRatio)
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

	CancelPattern();

	Phase = 2;
	OnPhaseChanged.Broadcast(Phase);
	ApplyBossBuff();

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
	ApplyBossBuff();
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
	GetWorldTimerManager().ClearTimer(ReturnTimeoutHandle);
	CancelPattern();
	DestroyMinions();
	ClearBossBuff();
	bFightActive = false;
	bInEntrance = false;
	bInPhaseTransition = false;
	bReturning = false;
	RegisterActiveBoss(false);

	MulticastBossEvent(ETDBossEvent::Death, 0);
	MulticastCameraShake(GetActorLocation(), 3.f);

	Super::HandleDeath();
}

// ── 방송 ─────────────────────────────────────────────────

void ATDBossCharacter::MulticastBossEvent_Implementation(ETDBossEvent Event, int32 Param)
{
	OnBossEvent.Broadcast(Event, Param);
}

void ATDBossCharacter::MulticastPatternTelegraph_Implementation(int32 PatternIndex, FVector Center, FVector Direction, float Duration)
{
	OnPatternTelegraph.Broadcast(PatternIndex, Center, Direction, Duration);

	// 예고 VFX: 스펙에 지정돼 있으면 각 머신이 직접 띄운다. 타격에 지운다.
	ClearTelegraphVFX();
	if (Patterns.IsValidIndex(PatternIndex))
	{
		const FTDBossPatternSpec& Spec = Patterns[PatternIndex];
		if (UNiagaraSystem* System = Spec.TelegraphVFX.LoadSynchronous())
		{
			TelegraphVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this, System, Center, Direction.Rotation(), FVector(Spec.TelegraphVFXScale),
				/*bAutoDestroy=*/ true, /*bAutoActivate=*/ true);
		}
	}
}

void ATDBossCharacter::MulticastPatternStrike_Implementation(int32 PatternIndex, FVector Center, FVector Direction)
{
	OnBossEvent.Broadcast(ETDBossEvent::PatternStrike, PatternIndex);

	ClearTelegraphVFX();
	if (Patterns.IsValidIndex(PatternIndex))
	{
		const FTDBossPatternSpec& Spec = Patterns[PatternIndex];
		if (UNiagaraSystem* System = Spec.StrikeVFX.LoadSynchronous())
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this, System, Center, Direction.Rotation(), FVector(Spec.StrikeVFXScale), true, true);
		}
	}
}

void ATDBossCharacter::ClearTelegraphVFX()
{
	if (TelegraphVFXComponent != nullptr)
	{
		TelegraphVFXComponent->DestroyComponent();
		TelegraphVFXComponent = nullptr;
	}
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