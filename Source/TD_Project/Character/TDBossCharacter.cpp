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
	HomeRotation = GetActorRotation();
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

	if (bDashing)         TickDash(DeltaSeconds);
	else if (bDiving)     TickDive(DeltaSeconds);
	else if (bBurrowed)   TickBurrow(DeltaSeconds);
	else if (bEmerging)   TickEmerge(DeltaSeconds);
	else if (bReturning)  TickReturn();

	// 선딜(재조준 포함) 동안 몸을 판정 방향으로 돌린다. 판정은 이미 고정 — 그림만 따라간다.
	if (CurrentPatternPhase == ETDBossPatternPhase::Telegraph && !bBurrowed && !bDiving)
	{
		TickTurn(DeltaSeconds);
	}
}

void ATDBossCharacter::TickTurn(float DeltaSeconds)
{
	const FRotator Current = GetActorRotation();
	const FRotator Goal(0.f, GetFacing().Rotation().Yaw, 0.f);
	const FRotator Next = FMath::RInterpConstantTo(FRotator(0.f, Current.Yaw, 0.f), Goal, DeltaSeconds, TurnSpeedDegrees);
	SetActorRotation(FRotator(Current.Pitch, Next.Yaw, Current.Roll));
}

// ── 조회 ─────────────────────────────────────────────────

bool ATDBossCharacter::IsBusy() const
{
	return CurrentPatternPhase != ETDBossPatternPhase::None || bInEntrance || bInPhaseTransition || bReturning;
}

bool ATDBossCharacter::IsInvulnerable() const
{
	// 시간 무적(방벽 스킬, 부모)에 보스 고유 무적을 더한다. Super 를 빼면 보스만 방벽이 안 먹는다.
	return Super::IsInvulnerable() || bInEntrance || bInPhaseTransition || bDiving || bBurrowed || bEmerging || bReturning;
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

FVector ATDBossCharacter::GetHomeNavLocation() const
{
	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	return HomeLocation - FVector(0.f, 0.f, HalfHeight);
}

float ATDBossCharacter::GetFloorZ() const
{
	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	// 가라앉는 중·땅속·솟구침 중엔 몸이 위아래로 움직이므로 잠수 전 캡슐 높이(BurrowTo.Z)를 기준으로 삼는다.
	const float CenterZ = (bDiving || bBurrowed || bEmerging) ? BurrowTo.Z : GetActorLocation().Z;
	return CenterZ - HalfHeight;
}

FVector ATDBossCharacter::GetFacing() const
{
	if (const UTDCombatComponent* Combat = GetCombatComponent())
	{
		const FVector Facing = Combat->GetFacingDirection().GetSafeNormal2D();
		if (!Facing.IsNearlyZero())
		{
			return Facing;
		}
	}
	return GetActorForwardVector().GetSafeNormal2D();
}

// ── 전투 시작·리셋 ────────────────────────────────────────

void ATDBossCharacter::BeginFight(ATDCharacterBase* FirstTarget)
{
	if (!HasAuthority() || bFightActive || bReturning || IsDead())
	{
		return;
	}

	bFightActive = true;
	FightStartTime = GetWorld()->GetTimeSeconds();
	NextPatternAllowedTime = 0.f;
	NextRetargetTime = FightStartTime + RetargetInterval;
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
	// 귀환 중: 무적, 판단 정지(IsBusy), 공격 안 함. 이동은 BT 의 귀환 가지(Move To HomeLocation)가 한다 —
	// 여기서 직접 MoveTo 를 걸면 트리의 이동 태스크와 같은 경로추종을 두고 경쟁한다.
	bReturning = true;
	NextPatternAllowedTime = 0.f;
	MulticastBossEvent(ETDBossEvent::Reset, 0);

	// 길이 막혀 못 오면 순간이동으로 마무리. 리시가 영원히 안 풀리는 것보다 낫다.
	GetWorldTimerManager().SetTimer(ReturnTimeoutHandle, [this]()
	{
		if (bReturning)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Boss] %s 귀환 타임아웃 → 순간이동"), *GetName());
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
	// 마지막 정렬: 항상 같은 자리·같은 방향. Z 는 지금 값을 둬 바닥에 묻히지 않게.
	SetActorLocation(FVector(HomeLocation.X, HomeLocation.Y, GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(HomeRotation);
	if (UTDCombatComponent* Combat = GetCombatComponent())
	{
		Combat->SetFacingDirection(HomeRotation.Vector());   // 스프라이트·판정 방향도 함께
	}

	bEnraged = false;
	LastPattern = INDEX_NONE;
	NextPatternAllowedTime = 0.f;
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
		GameState->SetActiveBoss(nullptr);
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
	ClearExtraStrikeTimers();
}

void ATDBossCharacter::ClearExtraStrikeTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TM = World->GetTimerManager();
		for (FTimerHandle& Handle : ExtraStrikeHandles)
		{
			TM.ClearTimer(Handle);
		}
	}
	ExtraStrikeHandles.Empty();
}

// ── 강화 (스탯 소스) ──────────────────────────────────────

void ATDBossCharacter::ApplyBossBuff()
{
	UTDStatComponent* Stats = GetStatComponent();
	if (Stats == nullptr)
	{
		return;
	}

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

ATDCharacterBase* ATDBossCharacter::FindRandomEnemy(float MaxRange, const ATDCharacterBase* Exclude) const
{
	TArray<ATDCharacterBase*> Candidates;
	for (TActorIterator<ATDCharacterBase> It(GetWorld()); It; ++It)
	{
		ATDCharacterBase* Candidate = *It;
		if (Candidate == nullptr || Candidate == this || Candidate->IsDead() ||
			Candidate->GetGenericTeamId() == GetGenericTeamId())
		{
			continue;
		}
		if (FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation()) > FMath::Square(MaxRange))
		{
			continue;
		}
		Candidates.Add(Candidate);
	}

	// 지금 대상은 뺀다 — "교체" 인데 같은 사람이 또 뽑히면 의미가 없다. 단, 후보가 그 한 명뿐이면 유지.
	if (Exclude != nullptr && Candidates.Num() > 1)
	{
		Candidates.Remove(const_cast<ATDCharacterBase*>(Exclude));
	}

	return Candidates.Num() > 0 ? Candidates[FMath::RandRange(0, Candidates.Num() - 1)] : nullptr;
}

bool ATDBossCharacter::ShouldRetargetNow()
{
	if (RetargetInterval <= 0.f || !bFightActive)
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextRetargetTime)
	{
		return false;
	}

	NextRetargetTime = Now + RetargetInterval;
	return true;
}

int32 ATDBossCharacter::ChoosePattern(float DistanceToTarget) const
{
	const float Now = GetWorld()->GetTimeSeconds();

	// 패턴 사이 쉼. 이 동안 트리는 접근/걷기 가지로 간다 — 쉴 새 없이 두들기지 않게.
	if (Now < NextPatternAllowedTime)
	{
		return INDEX_NONE;
	}

	TArray<int32> Candidates;
	float TotalWeight = 0.f;

	for (int32 Index = 0; Index < Patterns.Num(); ++Index)
	{
		const FTDBossPatternSpec& Spec = Patterns[Index];
		const bool bReady = !PatternReadyTime.IsValidIndex(Index) || Now >= PatternReadyTime[Index];
		const bool bInRange = DistanceToTarget >= Spec.MinDistance && DistanceToTarget <= Spec.MaxDistance;
		const bool bInPhase = Phase >= Spec.MinPhase && Phase <= Spec.MaxPhase;
		const bool bRepeat = bAvoidRepeatingPattern && Index == LastPattern && Patterns.Num() > 1;

		if (bReady && bInRange && bInPhase && !bRepeat && Spec.Weight > 0.f)
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

bool ATDBossCharacter::HasReadyPattern(float DistanceToTarget) const
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (IsBusy() || Now < NextPatternAllowedTime)
	{
		return false;
	}
	for (int32 Index = 0; Index < Patterns.Num(); ++Index)
	{
		const FTDBossPatternSpec& Spec = Patterns[Index];
		const bool bReady = !PatternReadyTime.IsValidIndex(Index) || Now >= PatternReadyTime[Index];
		const bool bInRange = DistanceToTarget >= Spec.MinDistance && DistanceToTarget <= Spec.MaxDistance;
		const bool bInPhase = Phase >= Spec.MinPhase && Phase <= Spec.MaxPhase;
		const bool bRepeat = bAvoidRepeatingPattern && Index == LastPattern && Patterns.Num() > 1;
		if (bReady && bInRange && bInPhase && !bRepeat && Spec.Weight > 0.f)
		{
			return true;
		}
	}
	return false;
}

// ── 패턴 엔진: 선딜 → 타격 → 후딜 ────────────────────────

bool ATDBossCharacter::StartPattern(int32 PatternIndex)
{
	if (!HasAuthority() || IsDead() || IsBusy() || !Patterns.IsValidIndex(PatternIndex))
	{
		return false;
	}

	const FTDBossPatternSpec& Spec = Patterns[PatternIndex];

	// 대상 쪽을 판정 방향으로 고정하고, 그 순간의 위치를 찍어둔다. 이후 대상이 움직여도 안 따라간다.
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
	DashesLeft = Spec.Motion == ETDBossMotion::Dash ? FMath::Max(1, Spec.DashCount) : 1;
	bExtrasScheduled = false;

	const float Duration = FMath::Max(ScaledTelegraph(Spec.TelegraphTime), 0.01f);
	const FVector Base = GetStrikeBase(Spec);

	MulticastBossEvent(ETDBossEvent::PatternTelegraph, PatternIndex);

	if (Spec.Motion == ETDBossMotion::Burrow)
	{
		// 잠수는 땅속 이동 동안 예고가 없다. 어디서 나올지는 이동이 끝난 뒤(EndBurrowTravel)에야 알려준다.
		OnTelegraphBegin(Spec);
		GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATDBossCharacter::EndBurrowTravel, Duration, false);
		return true;
	}

	MulticastPatternTelegraph(PatternIndex, Base, GetFacing(), Duration);
	DrawArea(MakePrimaryArea(Spec), Base, GetFacing(), FColor::Yellow, Duration);

	OnTelegraphBegin(Spec);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATDBossCharacter::EnterStrike, Duration, false);
	return true;
}

void ATDBossCharacter::EndBurrowTravel()
{
	if (!Patterns.IsValidIndex(CurrentPattern))
	{
		CancelPattern();
		return;
	}
	const FTDBossPatternSpec& Spec = Patterns[CurrentPattern];

	// 선딜이 가라앉는 시간보다 짧으면 아직 보이는 채일 수 있다 — 먼저 숨기고 땅속 상태로.
	if (bDiving)
	{
		FinishDive();
	}

	// 출현 자리는 **지금** 대상 위치. 잠수 중에 걸어 나간 만큼 따라간다 — 피할 시간은 아래 예고 시간뿐이다.
	ATDCharacterBase* Target = PatternTarget.Get();
	if (Target == nullptr || Target->IsDead())
	{
		Target = FindEnemy(false, LeashRadius);
	}
	if (Target != nullptr)
	{
		PatternTargetLocation = Target->GetActorLocation();
	}
	BurrowTo = FVector(PatternTargetLocation.X, PatternTargetLocation.Y, BurrowTo.Z);
	SetActorLocation(BurrowTo, false, nullptr, ETeleportType::TeleportPhysics);

	// 몸은 헤엄쳐 온 방향을 본다.
	if (UTDCombatComponent* Combat = GetCombatComponent())
	{
		const FVector Travel = BurrowTo - BurrowFrom;
		if (!Travel.IsNearlyZero())
		{
			Combat->SetFacingDirection(Travel);
		}
	}

	const float Duration = FMath::Max(ScaledTelegraph(Spec.EmergeTelegraphTime), 0.05f);
	const FVector Base = GetStrikeBase(Spec);
	MulticastPatternTelegraph(CurrentPattern, Base, GetFacing(), Duration);
	DrawArea(MakePrimaryArea(Spec), Base, GetFacing(), FColor::Yellow, Duration);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATDBossCharacter::EnterStrike, Duration, false);
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

	OnMotionBegin(Spec);

	const FVector Base = GetStrikeBase(Spec);
	const FVector Facing = GetFacing();

	// 잠수는 타격 방송·흔들림을 솟구침 정점에서 한다(TickEmerge). 예고 원이 맞는 순간까지 남아 있게.
	if (Spec.Motion != ETDBossMotion::Burrow)
	{
		MulticastPatternStrike(CurrentPattern, Base, Facing);
		if (Spec.ShakeScale > 0.f)
		{
			MulticastCameraShake(Base, Spec.ShakeScale);
		}
	}

	// 투사체는 탄이, 잠수는 솟구침 정점에서 판정한다. 나머지는 여기서 — 한 번 또는 반복.
	if (Spec.Motion != ETDBossMotion::Projectile && Spec.Motion != ETDBossMotion::Burrow)
	{
		DoStrikeHit();
		if (Spec.StrikeInterval > 0.f && Spec.StrikeTime > Spec.StrikeInterval)
		{
			GetWorldTimerManager().SetTimer(StrikeTickHandle, this, &ATDBossCharacter::DoStrikeHit,
				Spec.StrikeInterval, true);
		}
	}

	// 추가 타격 예고. 잠수는 정점에서 건다(TickEmerge). 연속 돌진은 첫 돌진에만 — bExtrasScheduled 가 막는다.
	if (Spec.Motion != ETDBossMotion::Burrow)
	{
		ScheduleExtraStrikes(CurrentPattern, Base, Facing);
	}

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATDBossCharacter::EnterRecovery,
		FMath::Max(Spec.StrikeTime, 0.01f), false);
}

FVector ATDBossCharacter::GetStrikeBase(const FTDBossPatternSpec& Spec) const
{
	// 높이는 발밑. 잠수는 대상 자리(출현 지점), 나머지는 보스 자리. 갈래·오프셋은 영역 계산이 더한다.
	const FVector XY = Spec.Motion == ETDBossMotion::Burrow ? PatternTargetLocation : GetActorLocation();
	return FVector(XY.X, XY.Y, GetFloorZ());
}

void ATDBossCharacter::DoStrikeHit()
{
	if (!Patterns.IsValidIndex(CurrentPattern) || IsDead())
	{
		return;
	}
	const FTDBossPatternSpec& Spec = Patterns[CurrentPattern];
	const FTDBossHitArea Area = MakePrimaryArea(Spec);
	const FVector Base = GetStrikeBase(Spec);
	const FVector Facing = GetFacing();

	// 밀어내기 높이: 잠수(솟구침)는 크게 띄우고, 원판·도넛은 중간, 박스는 낮게.
	const float UpRatio = Spec.Motion == ETDBossMotion::Burrow ? 1.2f
		: (Area.Shape == ETDBossHitShape::Box ? 0.4f : 0.8f);

	StrikeArea(Area, Base, Facing, Spec.DamageScale, Spec.Knockback, UpRatio, &HitThisStrike);
	DrawArea(Area, Base, Facing, FColor::Red, 0.5f);
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

	// 연속 돌진: 후딜 대신 재조준 → 다시 타격.
	if (Spec.Motion == ETDBossMotion::Dash && DashesLeft > 1)
	{
		--DashesLeft;
		StartDashReaim();
		return;
	}

	OnMotionEnd(Spec);
	CurrentPatternPhase = ETDBossPatternPhase::Recovery;
	MulticastBossEvent(ETDBossEvent::PatternRecovery, CurrentPattern);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATDBossCharacter::FinishPattern,
		FMath::Max(ScaledRecovery(Spec.RecoveryTime), 0.01f), false);
}

void ATDBossCharacter::StartDashReaim()
{
	const FTDBossPatternSpec& Spec = Patterns[CurrentPattern];

	EndDash(false);
	CurrentPatternPhase = ETDBossPatternPhase::Telegraph;   // TickTurn 이 몸을 돌린다

	// 돌진이 끝난 자리에서 대상의 **지금** 위치로 다시 조준한다.
	if (ATDCharacterBase* Target = PatternTarget.Get())
	{
		if (!Target->IsDead())
		{
			PatternTargetLocation = Target->GetActorLocation();
			if (UTDCombatComponent* Combat = GetCombatComponent())
			{
				Combat->SetFacingDirection(PatternTargetLocation - GetActorLocation());
			}
		}
	}

	const float Duration = FMath::Max(Spec.DashReaimTime, 0.05f);
	const FVector Base = GetStrikeBase(Spec);
	MulticastBossEvent(ETDBossEvent::PatternTelegraph, CurrentPattern);
	MulticastPatternTelegraph(CurrentPattern, Base, GetFacing(), Duration);
	DrawArea(MakePrimaryArea(Spec), Base, GetFacing(), FColor::Yellow, Duration);

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATDBossCharacter::EnterStrike, Duration, false);
}

void ATDBossCharacter::FinishPattern()
{
	const int32 Finished = CurrentPattern;
	if (Patterns.IsValidIndex(Finished) && PatternReadyTime.IsValidIndex(Finished))
	{
		PatternReadyTime[Finished] = GetWorld()->GetTimeSeconds() + Patterns[Finished].Cooldown;
	}

	// 다음 패턴까지의 쉼(랜덤). 이 사이에 보스가 걷고 재배치한다.
	NextPatternAllowedTime = GetWorld()->GetTimeSeconds()
		+ FMath::FRandRange(PatternGapMin, FMath::Max(PatternGapMin, PatternGapMax));

	LastPattern = Finished;
	CurrentPattern = INDEX_NONE;
	CurrentPatternPhase = ETDBossPatternPhase::None;
	// 아직 안 떨어진 추가 타격(물의 폭포 등)은 그대로 둔다 — 자기 타이머로 떨어진다.
	MulticastBossEvent(ETDBossEvent::PatternEnd, Finished);
	OnPatternFinished.Broadcast(Finished);
}

void ATDBossCharacter::CancelPattern()
{
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
	GetWorldTimerManager().ClearTimer(StrikeTickHandle);
	EndDash(false);
	AbortBurrow();

	// 취소는 예정된 추가 타격도 같이 지운다. 죽은 보스의 폭포가 떨어지면 안 된다.
	const bool bHadExtras = ExtraStrikeHandles.Num() > 0;
	ClearExtraStrikeTimers();
	bExtrasScheduled = false;

	if (CurrentPatternPhase == ETDBossPatternPhase::None)
	{
		if (bHadExtras)
		{
			MulticastClearTelegraphs();
		}
		return;
	}

	const int32 Cancelled = CurrentPattern;
	CurrentPattern = INDEX_NONE;
	CurrentPatternPhase = ETDBossPatternPhase::None;
	MulticastClearTelegraphs();
	MulticastBossEvent(ETDBossEvent::PatternEnd, Cancelled);
	OnPatternFinished.Broadcast(Cancelled);
}

float ATDBossCharacter::ScaledTelegraph(float Base) const
{
	float Scale = TelegraphScale;
	if (Phase >= 2) Scale *= Phase2TelegraphScale;
	if (bEnraged)   Scale *= EnrageTelegraphScale;
	return Base * Scale;
}

float ATDBossCharacter::ScaledRecovery(float Base) const
{
	return Base * (Phase >= 2 ? Phase2RecoveryScale : 1.f);
}

void ATDBossCharacter::ApplyKnockback(ATDCharacterBase* Target, const FVector& Direction, float Strength, float UpRatio) const
{
	if (Target == nullptr || Strength <= 0.f)
	{
		return;
	}
	const FVector Flat = Direction.GetSafeNormal2D();
	Target->LaunchCharacter(Flat * Strength + FVector(0.f, 0.f, Strength * UpRatio), true, true);
}

// ── 판정 영역 ────────────────────────────────────────────

FTDBossHitArea ATDBossCharacter::MakePrimaryArea(const FTDBossPatternSpec& Spec)
{
	FTDBossHitArea Area;
	Area.Shape = Spec.Shape;
	Area.BoxExtent = Spec.BoxExtent;
	Area.BoxDirections = Spec.BoxDirections;
	Area.SphereRadius = Spec.SphereRadius;
	Area.RingInnerRadius = Spec.RingInnerRadius;
	Area.RingOuterRadius = Spec.RingOuterRadius;
	Area.ForwardOffset = Spec.ForwardOffset;
	return Area;
}

TArray<ATDCharacterBase*> ATDBossCharacter::GatherInArea(const FTDBossHitArea& Area, const FVector& Center, const FVector& Facing) const
{
	TArray<ATDCharacterBase*> Result;
	if (GetWorld() == nullptr)
	{
		return Result;
	}

	FVector Flat = Facing.GetSafeNormal2D();
	if (Flat.IsNearlyZero())
	{
		Flat = GetActorForwardVector().GetSafeNormal2D();
	}

	if (Area.Shape == ETDBossHitShape::Box)
	{
		// 갈래마다 상자 하나. 상자는 바닥에 반쯤 묻혀 플레이어 캡슐(0~180)을 확실히 덮는다.
		TSet<ATDCharacterBase*> Seen;
		const int32 Count = FMath::Max(1, Area.BoxDirections);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Dir = Flat.RotateAngleAxis(360.f / Count * Index, FVector::UpVector);
			const FVector BoxCenter = Center + Dir * Area.ForwardOffset + FVector(0.f, 0.f, Area.BoxExtent.Z * 0.5f);
			const FQuat Rot = FRotationMatrix::MakeFromX(Dir).ToQuat();
			for (ATDCharacterBase* Found : UTDCombatStatics::GatherEnemiesInBox(this, BoxCenter, Rot, Area.BoxExtent, false))
			{
				if (!Seen.Contains(Found))
				{
					Seen.Add(Found);
					Result.Add(Found);
				}
			}
		}
		return Result;
	}

	// 원판·도넛: 바닥 기준 원기둥. 넉넉한 구로 모은 뒤 XY 거리로 거른다 — 예고 원과 맞는 범위가 정확히 같다.
	const bool bRing = Area.Shape == ETDBossHitShape::Ring;
	const float Outer = bRing ? Area.RingOuterRadius : Area.SphereRadius;
	const float Inner = bRing ? Area.RingInnerRadius : 0.f;
	if (Outer <= 0.f)
	{
		return Result;
	}
	const FVector AreaCenter = Center + Flat * Area.ForwardOffset;
	const FVector QueryCenter = AreaCenter + FVector(0.f, 0.f, HitCylinderHalfHeight);

	for (ATDCharacterBase* Found : UTDCombatStatics::GatherEnemiesInSphere(this, QueryCenter, Outer + HitCylinderHalfHeight, false))
	{
		const FVector Loc = Found->GetActorLocation();
		const float Dist = FVector::Dist2D(Loc, AreaCenter);
		const float DZ = Loc.Z - AreaCenter.Z;
		if (Dist >= Inner && Dist <= Outer && DZ >= -HitCylinderHalfHeight && DZ <= HitCylinderHalfHeight * 2.f)
		{
			Result.Add(Found);
		}
	}
	return Result;
}

void ATDBossCharacter::StrikeArea(const FTDBossHitArea& Area, const FVector& Center, const FVector& Facing,
	float DamageScale, float Knockback, float KnockUpRatio, TSet<TWeakObjectPtr<AActor>>* AlreadyHit)
{
	UTDCombatComponent* Combat = GetCombatComponent();
	const FVector AreaCenter = Center + Facing.GetSafeNormal2D() * (Area.Shape == ETDBossHitShape::Box ? 0.f : Area.ForwardOffset);

	for (ATDCharacterBase* Enemy : GatherInArea(Area, Center, Facing))
	{
		if (AlreadyHit != nullptr && AlreadyHit->Contains(Enemy))
		{
			continue;
		}
		const FTDDamageResult Result = UTDCombatStatics::ApplyDamage(this, Enemy, FGameplayTagContainer(), DamageScale);
		if (Result.FinalDamage <= 0.f)
		{
			continue;
		}
		if (AlreadyHit != nullptr)
		{
			AlreadyHit->Add(Enemy);
		}
		if (Combat != nullptr)
		{
			Combat->NotifyHit(Enemy, Result.FinalDamage, Result.bCritical, Enemy->GetActorLocation());
		}
		// 밀어내기: 판정 중심에서 바깥으로. 중심에 딱 서 있으면 정면으로.
		FVector Away = (Enemy->GetActorLocation() - AreaCenter).GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			Away = Facing.GetSafeNormal2D();
		}
		ApplyKnockback(Enemy, Away, Knockback, KnockUpRatio);
	}
}

void ATDBossCharacter::DrawArea(const FTDBossHitArea& Area, const FVector& Center, const FVector& Facing, const FColor& Color, float Duration) const
{
#if ENABLE_DRAW_DEBUG
	if (!bDrawDebugHits || GetWorld() == nullptr)
	{
		return;
	}
	FVector Flat = Facing.GetSafeNormal2D();
	if (Flat.IsNearlyZero())
	{
		Flat = GetActorForwardVector().GetSafeNormal2D();
	}

	if (Area.Shape == ETDBossHitShape::Box)
	{
		const int32 Count = FMath::Max(1, Area.BoxDirections);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Dir = Flat.RotateAngleAxis(360.f / Count * Index, FVector::UpVector);
			const FVector BoxCenter = Center + Dir * Area.ForwardOffset + FVector(0.f, 0.f, Area.BoxExtent.Z * 0.5f);
			DrawDebugBox(GetWorld(), BoxCenter, Area.BoxExtent, FRotationMatrix::MakeFromX(Dir).ToQuat(), Color, false, Duration, 0, 3.f);
		}
		return;
	}

	// 원판·도넛은 바닥에 원으로 그린다 — 실제 맞는 범위(XY 거리)와 같은 그림.
	const FVector AreaCenter = Center + Flat * Area.ForwardOffset + FVector(0.f, 0.f, 5.f);
	const bool bRing = Area.Shape == ETDBossHitShape::Ring;
	const float Outer = bRing ? Area.RingOuterRadius : Area.SphereRadius;
	DrawDebugCircle(GetWorld(), AreaCenter, Outer, 48, Color, false, Duration, 0, 4.f, FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
	if (bRing && Area.RingInnerRadius > 0.f)
	{
		DrawDebugCircle(GetWorld(), AreaCenter, Area.RingInnerRadius, 48, Color, false, Duration, 0, 4.f, FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
	}
#endif
}

void ATDBossCharacter::SpawnAreaVFX(const FTDBossHitArea& Area, const FVector& Center, const FVector& Facing,
	UNiagaraSystem* System, float Scale, float HeightScale, int32 PointCount, int32 RingLayers, float ReferenceSize,
	float HeightOffset, float Duration, float AutoKillAfter, TArray<TObjectPtr<UNiagaraComponent>>* KeepIn)
{
	if (System == nullptr || GetWorld() == nullptr)
	{
		return;
	}
	FVector Flat = Facing.GetSafeNormal2D();
	if (Flat.IsNearlyZero())
	{
		Flat = GetActorForwardVector().GetSafeNormal2D();
	}

	// 지점 여러 개로 "채우기": 큰 판정을 작은 이펙트 하나로 늘리면 흐려지니, 여러 개를 타일처럼 깐다.
	//  - Box   : 길이(X) 방향으로 PointCount 칸. 각 칸이 길이/PointCount 를 덮는다.
	//  - Sphere: 중심 하나 + 링 줄(RingLayers)마다 둘레에 PointCount 비례 개수.
	//  - Ring  : 줄(RingLayers)마다 둘레에 PointCount 비례 개수.
	const bool bBox = Area.Shape == ETDBossHitShape::Box;
	const bool bRing = Area.Shape == ETDBossHitShape::Ring;
	const bool bTiled = PointCount > 0;
	const int32 RingLayerCount = (bTiled && !bBox) ? FMath::Max(1, RingLayers) : 1;
	const float Outer = bRing ? Area.RingOuterRadius : Area.SphereRadius;
	const float Inner = bRing ? Area.RingInnerRadius : 0.f;

	// 이펙트 하나가 덮어야 할 크기. User 파라미터로도 넘기고, 기준 크기가 있으면 배율도 여기서 계산한다.
	const float TileHalfX = (bBox && bTiled) ? Area.BoxExtent.X / PointCount : Area.BoxExtent.X;
	const float RadialSize = (bTiled && !bBox) ? (Outer - Inner) * 0.5f / RingLayerCount : Outer;
	FVector FinalScale(Scale);
	if (ReferenceSize > 0.f)
	{
		if (bBox)
		{
			FinalScale = FVector(TileHalfX / ReferenceSize, Area.BoxExtent.Y / ReferenceSize, 1.f) * Scale;
		}
		else
		{
			FinalScale = FVector(RadialSize / ReferenceSize) * Scale;
		}
	}
	FinalScale.Z *= FMath::Max(HeightScale, 0.01f);   // 세로만 따로 — 물기둥 높이, 파도 높이

	auto SpawnAt = [&](const FVector& Location, const FVector& Dir)
	{
		// 높이 오프셋은 배율과 무관하게 cm 그대로 — 원점이 가운데인 이펙트를 바닥 위로 올리는 용도.
		UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, System, Location + FVector(0.f, 0.f, HeightOffset), Dir.Rotation(), FinalScale, true, true);
		if (Comp == nullptr)
		{
			return;
		}

		// 이펙트가 이 이름의 User 파라미터를 갖고 있으면 값이 들어가고, 없으면 조용히 무시된다.
		Comp->SetVariableFloat(FName(TEXT("Radius")), RadialSize);
		Comp->SetVariableFloat(FName(TEXT("InnerRadius")), Inner);
		Comp->SetVariableFloat(FName(TEXT("Length")), TileHalfX * 2.f);
		Comp->SetVariableFloat(FName(TEXT("Width")), Area.BoxExtent.Y * 2.f);
		Comp->SetVariableFloat(FName(TEXT("Duration")), Duration);

		if (KeepIn != nullptr)
		{
			KeepIn->Add(Comp);
		}
		if (AutoKillAfter > 0.f)
		{
			// 루프 이펙트는 스스로 안 끝난다. 시간이 되면 끄고(남은 입자는 자연 소멸) 자동 파괴에 맡긴다.
			TWeakObjectPtr<UNiagaraComponent> WeakComp = Comp;
			FTimerHandle Handle;
			GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [WeakComp]()
			{
				if (WeakComp.IsValid())
				{
					WeakComp->Deactivate();
				}
			}), AutoKillAfter, false);
		}
	};

	if (bBox)
	{
		const int32 Count = FMath::Max(1, Area.BoxDirections);
		const int32 Tiles = bTiled ? PointCount : 1;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Dir = Flat.RotateAngleAxis(360.f / Count * Index, FVector::UpVector);
			const FVector BoxCenter = Center + Dir * Area.ForwardOffset;
			for (int32 Tile = 0; Tile < Tiles; ++Tile)
			{
				// 박스 뒤끝(-X)부터 앞끝(+X)까지 칸 가운데에 하나씩.
				const float Along = -Area.BoxExtent.X + TileHalfX * (2 * Tile + 1);
				SpawnAt(BoxCenter + Dir * Along, Dir);
			}
		}
		return;
	}

	const FVector AreaCenter = Center + Flat * Area.ForwardOffset;
	if (!bTiled)
	{
		SpawnAt(AreaCenter, Flat);
		return;
	}

	// 원판은 중심에도 하나. 링은 안쪽이 비어 있으니 중심은 건너뛴다.
	if (!bRing)
	{
		SpawnAt(AreaCenter, Flat);
	}

	// 링 두께를 줄로 나눠 각 줄의 가운데 반지름에 균등 배치. 바깥 줄일수록 둘레가 기니까
	// 지점 수를 반지름에 비례해 늘린다 — PointCount 는 "가운데 줄" 기준. 줄마다 반 칸씩 엇갈려 빈틈을 줄인다.
	const float Mid = (Inner + Outer) * 0.5f;
	const float LayerThickness = (Outer - Inner) / RingLayerCount;
	for (int32 Layer = 0; Layer < RingLayerCount; ++Layer)
	{
		const float R = Inner + LayerThickness * (Layer + 0.5f);
		const int32 Count = Mid > KINDA_SMALL_NUMBER
			? FMath::Max(1, FMath::RoundToInt(PointCount * R / Mid))
			: PointCount;
		const float Stagger = (Layer % 2) * 180.f / Count;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Dir = Flat.RotateAngleAxis(360.f / Count * Index + Stagger, FVector::UpVector);
			SpawnAt(AreaCenter + Dir * R, Dir);
		}
	}
}

void ATDBossCharacter::ScheduleExtraStrikes(int32 PatternIndex, const FVector& PrimaryBase, const FVector& Facing)
{
	if (bExtrasScheduled || !Patterns.IsValidIndex(PatternIndex))
	{
		return;
	}
	bExtrasScheduled = true;

	const FTDBossPatternSpec& Spec = Patterns[PatternIndex];
	for (int32 ExtraIndex = 0; ExtraIndex < Spec.ExtraStrikes.Num(); ++ExtraIndex)
	{
		const FTDBossExtraStrike& Extra = Spec.ExtraStrikes[ExtraIndex];
		const FVector Center = Extra.bCenterOnBoss
			? FVector(GetActorLocation().X, GetActorLocation().Y, GetFloorZ())
			: PrimaryBase;
		const float Delay = FMath::Max(Extra.Delay, 0.01f);

		MulticastExtraTelegraph(PatternIndex, ExtraIndex, Center, Facing, Delay);
		DrawArea(Extra.Area, Center, Facing, FColor::Yellow, Delay);

		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle,
			FTimerDelegate::CreateWeakLambda(this, [this, PatternIndex, ExtraIndex, Center, Facing]()
			{
				DoExtraStrike(PatternIndex, ExtraIndex, Center, Facing);
			}), Delay, false);
		ExtraStrikeHandles.Add(Handle);
	}
}

void ATDBossCharacter::DoExtraStrike(int32 PatternIndex, int32 ExtraIndex, FVector Center, FVector Facing)
{
	if (IsDead() || !Patterns.IsValidIndex(PatternIndex) || !Patterns[PatternIndex].ExtraStrikes.IsValidIndex(ExtraIndex))
	{
		return;
	}
	const FTDBossExtraStrike& Extra = Patterns[PatternIndex].ExtraStrikes[ExtraIndex];

	const float UpRatio = Extra.Area.Shape == ETDBossHitShape::Box ? 0.4f : 1.0f;
	StrikeArea(Extra.Area, Center, Facing, Extra.DamageScale, Extra.Knockback, UpRatio, nullptr);
	DrawArea(Extra.Area, Center, Facing, FColor::Red, 0.5f);

	MulticastExtraStrike(PatternIndex, ExtraIndex, Center, Facing);
	if (Extra.ShakeScale > 0.f)
	{
		MulticastCameraShake(Center, Extra.ShakeScale);
	}
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
	case ETDBossMotion::Burrow:     StartEmerge();          break;
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

void ATDBossCharacter::SetMovementFrozen(bool bFrozen)
{
	// 땅속·공중에서는 캐릭터 무브먼트가 바닥을 찾아 끌어내리지 않게 잠근다.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (bFrozen)
		{
			Movement->StopMovementImmediately();
			Movement->SetMovementMode(MOVE_None);
		}
		else
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
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

	if (!Hit.bBlockingHit)
	{
		return;
	}

	// 캐릭터에 부딪히면 밀어내고 계속 달린다. 겹치지 않고 튕겨 나가는 게 "부딪혔다"의 그림이다.
	if (ATDCharacterBase* Blocker = Cast<ATDCharacterBase>(Hit.GetActor()))
	{
		if (Blocker->GetGenericTeamId() != GetGenericTeamId())
		{
			ApplyKnockback(Blocker, DashDirection, DashShove, 0.3f);
		}
		return;
	}

	EndDash(true);   // 벽
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

		// 벽에 막혔으면 남은 타격 시간을 기다리지 않는다. 연속 돌진이면 바로 재조준으로.
		if (CurrentPatternPhase == ETDBossPatternPhase::Strike)
		{
			GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
			EnterRecovery();
		}
	}
}

// ── 잠수 → 솟구침 ─────────────────────────────────────────

void ATDBossCharacter::StartBurrow(const FTDBossPatternSpec& Spec)
{
	// 목적지는 캡슐 중심 높이(착지가 안 묻히게). 판정 높이는 GetFloorZ 가 따로 계산한다.
	BurrowFrom = GetActorLocation();
	BurrowTo = FVector(PatternTargetLocation.X, PatternTargetLocation.Y, GetActorLocation().Z);

	// 선딜 = 가라앉기(보임) + 땅속 이동(숨김). 가라앉기가 선딜보다 길면 남는 이동 시간은 0 에 가깝다.
	const float Telegraph = FMath::Max(ScaledTelegraph(Spec.TelegraphTime), 0.01f);
	const float Dive = FMath::Min(DiveTime, Telegraph);
	BurrowElapsed = 0.f;
	BurrowDuration = FMath::Max(Telegraph - Dive, 0.01f);

	// 가라앉는 동안: 안 움직이고, 안 부딪히고, 안 맞는다(IsInvulnerable). 아직 보인다.
	SetMovementFrozen(true);
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 물보라: 발밑 바닥에 한 번.
	if (UNiagaraSystem* DiveSystem = DiveVFX.LoadSynchronous())
	{
		MulticastOneShotVFX(DiveSystem, FVector(BurrowFrom.X, BurrowFrom.Y, GetFloorZ()), DiveVFXScale, DiveVFXDuration);
	}

	if (Dive <= 0.f)
	{
		FinishDive();
		return;
	}
	bDiving = true;
	DiveElapsed = 0.f;
}

void ATDBossCharacter::TickDive(float DeltaSeconds)
{
	DiveElapsed += DeltaSeconds;
	const float A = FMath::Clamp(DiveElapsed / FMath::Max(DiveTime, 0.01f), 0.f, 1.f);

	// 점점 빨라지며 가라앉고(A²), 머리는 아래로 기운다.
	const float Z = FMath::Lerp(0.f, -EmergeDepth, A * A);
	SetActorLocation(BurrowFrom + FVector(0.f, 0.f, Z), false, nullptr, ETeleportType::TeleportPhysics);
	const float Yaw = GetActorRotation().Yaw;
	SetActorRotation(FRotator(-DiveTiltDegrees * A, Yaw, 0.f));

	if (A >= 1.f)
	{
		FinishDive();
	}
}

void ATDBossCharacter::FinishDive()
{
	// 땅속: 숨기고(복제되는 속성이라 클라도 따라온다) 이동 시작점 높이로 돌려놓는다. 기울기도 원복.
	bDiving = false;
	bBurrowed = true;
	SetActorHiddenInGame(true);
	SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
	SetActorLocation(BurrowFrom, false, nullptr, ETeleportType::TeleportPhysics);
}

void ATDBossCharacter::TickBurrow(float DeltaSeconds)
{
	BurrowElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(BurrowElapsed / BurrowDuration, 0.f, 1.f);
	SetActorLocation(FMath::Lerp(BurrowFrom, BurrowTo, Alpha), false, nullptr, ETeleportType::TeleportPhysics);
}

void ATDBossCharacter::StartEmerge()
{
	// 출현: 대상 자리 땅속에서 시작해 튀어오른다. 콜리전은 착지 때 켠다 — 공중에서 겹침 판정을 안 하려고.
	bBurrowed = false;
	bEmerging = true;
	bEmergeHitDone = false;
	EmergeElapsed = 0.f;
	SetActorLocation(BurrowTo - FVector(0.f, 0.f, EmergeDepth), false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(FRotator(0.f, GetFacing().Rotation().Yaw, 0.f));
	SetActorHiddenInGame(false);
}

void ATDBossCharacter::TickEmerge(float DeltaSeconds)
{
	EmergeElapsed += DeltaSeconds;

	float Z;
	if (EmergeElapsed < EmergeRiseTime)
	{
		// 상승: 땅속 → 정점. 갈수록 느려지는 곡선.
		const float A = EmergeElapsed / EmergeRiseTime;
		Z = FMath::Lerp(-EmergeDepth, EmergeHeight, FMath::Sin(A * HALF_PI));
	}
	else
	{
		// 정점 도달 순간 한 번: 판정 + 위로 띄우기 + 큰 흔들림 + 추가 타격 예고(물의 폭포).
		if (!bEmergeHitDone)
		{
			bEmergeHitDone = true;
			if (Patterns.IsValidIndex(CurrentPattern))
			{
				const FTDBossPatternSpec& Spec = Patterns[CurrentPattern];
				MulticastPatternStrike(CurrentPattern, GetStrikeBase(Spec), GetFacing());   // 예고 지우고 타격 VFX
			}
			DoStrikeHit();
			if (Patterns.IsValidIndex(CurrentPattern))
			{
				const FTDBossPatternSpec& Spec = Patterns[CurrentPattern];
				MulticastCameraShake(BurrowTo, Spec.ShakeScale);
				ScheduleExtraStrikes(CurrentPattern, GetStrikeBase(Spec), GetFacing());
			}
		}
		const float A = FMath::Clamp((EmergeElapsed - EmergeRiseTime) / EmergeFallTime, 0.f, 1.f);
		Z = FMath::Lerp(EmergeHeight, 0.f, A * A);   // 하강: 점점 빨라짐
		if (A >= 1.f)
		{
			FinishEmerge();
			return;
		}
	}
	SetActorLocation(BurrowTo + FVector(0.f, 0.f, Z), false, nullptr, ETeleportType::TeleportPhysics);
}

void ATDBossCharacter::FinishEmerge()
{
	bEmerging = false;

	// 착지. 아직 겹친 캐릭터가 있으면 겹치지 않는 가장 가까운 자리로 밀려난다.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	FVector Spot = BurrowTo;
	if (GetWorld()->FindTeleportSpot(this, Spot, GetActorRotation()))
	{
		SetActorLocation(Spot, false, nullptr, ETeleportType::TeleportPhysics);
	}
	SetMovementFrozen(false);
}

void ATDBossCharacter::AbortBurrow()
{
	// 취소·사망·리셋: 어느 단계든 즉시 지상으로.
	if (!bDiving && !bBurrowed && !bEmerging)
	{
		return;
	}
	// 가라앉다 취소되면 BurrowTo 는 아직 대상 자리다 — 출발 자리로 되돌린다. 기울기도 원복.
	if (bDiving)
	{
		BurrowTo = BurrowFrom;
	}
	bDiving = false;
	bBurrowed = false;
	bEmerging = false;
	SetActorHiddenInGame(false);
	SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	FVector Spot = BurrowTo;
	if (GetWorld()->FindTeleportSpot(this, Spot, GetActorRotation()))
	{
		SetActorLocation(Spot, false, nullptr, ETeleportType::TeleportPhysics);
	}
	SetMovementFrozen(false);
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

	// 좌우 조준은 선딜에 굳힌 Facing 그대로. 위아래만 대상 몸통 높이로 기울인다 —
	// 총구가 입(캡슐 중심보다 위)에 있으면 수평으로 쏠 때 작은 캐릭터 머리 위로 지나가 버린다.
	FVector Aim = Facing;
	{
		const ATDCharacterBase* Target = PatternTarget.Get();
		const FVector TargetPoint = (Target != nullptr && !Target->IsDead()) ? Target->GetActorLocation() : PatternTargetLocation;
		const float Flat = FVector::Dist2D(Muzzle, TargetPoint);
		if (Flat > KINDA_SMALL_NUMBER)
		{
			const float Pitch = FMath::Atan2(TargetPoint.Z - Muzzle.Z, Flat);
			Aim = Facing * FMath::Cos(Pitch) + FVector(0.f, 0.f, FMath::Sin(Pitch));
		}
	}

	// 탄 수: 스펙이 정했으면 그 값, 아니면 페이즈 규칙.
	const int32 Count = Spec.ProjectileCount > 0
		? Spec.ProjectileCount
		: (Phase >= 2 ? FMath::Max(1, ProjectileCountPhase2) : 1);
	const float StartAngle = -ProjectileSpreadAngle * (Count - 1) * 0.5f;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector Direction = Aim.RotateAngleAxis(StartAngle + ProjectileSpreadAngle * Index, FVector::UpVector);
		ATDBossProjectile* Projectile = GetWorld()->SpawnActor<ATDBossProjectile>(
			ProjectileClass, Muzzle, Direction.Rotation(), Params);
		if (Projectile != nullptr)
		{
			Projectile->Init(this, Direction, Spec.DamageScale, ProjectileSpeed, Spec.Knockback);
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

		// 소환수도 보스와 같은 그룹이다. 물려주지 않으면 그룹 키가 비어 공용이 되어
		// 남의 화면에 보이고 남을 때리게 된다.
		Minion->SetOwnerGroupId(GetOwnerGroupId());

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
	CancelPattern();   // 돌진·잠수·솟구침 상태와 예정된 추가 타격도 여기서 원복된다
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
	UE_LOG(LogTemp, Log, TEXT("[Boss] %s  %s  Param=%d"), *GetName(), *UEnum::GetValueAsString(Event), Param);
	OnBossEvent.Broadcast(Event, Param);
}

void ATDBossCharacter::MulticastPatternTelegraph_Implementation(int32 PatternIndex, FVector Center, FVector Direction, float Duration)
{
	UE_LOG(LogTemp, Log, TEXT("[Boss] %s  Telegraph %d  at %s  for %.2fs"),
		*GetName(), PatternIndex, *Center.ToCompactString(), Duration);

	ClearTelegraphVFX();
	if (!Patterns.IsValidIndex(PatternIndex))
	{
		OnPatternTelegraph.Broadcast(PatternIndex, Center, Direction, Duration);
		return;
	}
	const FTDBossPatternSpec& Spec = Patterns[PatternIndex];
	const FTDBossHitArea Area = MakePrimaryArea(Spec);

	// BP 에는 첫 갈래의 실제 판정 중심(기준점 + 정면 오프셋)을 준다 — 예전 계약 그대로.
	OnPatternTelegraph.Broadcast(PatternIndex, Center + Direction.GetSafeNormal2D() * Spec.ForwardOffset, Direction, Duration);

	if (UNiagaraSystem* System = Spec.TelegraphVFX.LoadSynchronous())
	{
		SpawnAreaVFX(Area, Center, Direction, System, Spec.TelegraphVFXScale, Spec.VFXHeightScale, Spec.VFXPointCount, Spec.VFXRingLayers,
			Spec.VFXReferenceSize, Spec.VFXHeightOffset, Duration, 0.f, &TelegraphVFXComponents);
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
			SpawnAreaVFX(MakePrimaryArea(Spec), Center, Direction, System, Spec.StrikeVFXScale, Spec.VFXHeightScale, Spec.VFXPointCount, Spec.VFXRingLayers,
				Spec.VFXReferenceSize, Spec.VFXHeightOffset, Spec.StrikeVFXDuration, Spec.StrikeVFXDuration, nullptr);
		}
	}
}

void ATDBossCharacter::MulticastExtraTelegraph_Implementation(int32 PatternIndex, int32 ExtraIndex, FVector Center, FVector Direction, float Duration)
{
	if (!Patterns.IsValidIndex(PatternIndex) || !Patterns[PatternIndex].ExtraStrikes.IsValidIndex(ExtraIndex))
	{
		return;
	}
	const FTDBossExtraStrike& Extra = Patterns[PatternIndex].ExtraStrikes[ExtraIndex];
	UE_LOG(LogTemp, Log, TEXT("[Boss] %s  Extra %d/%d (%s) telegraph at %s, lands in %.2fs"),
		*GetName(), PatternIndex, ExtraIndex, *Extra.Name.ToString(), *Center.ToCompactString(), Duration);

	// BP 에는 판정 중심(기준점 + 정면 오프셋)을 준다. 본 예고(OnPatternTelegraph)와 같은 약속.
	OnExtraTelegraph.Broadcast(PatternIndex, ExtraIndex,
		Center + Direction.GetSafeNormal2D() * Extra.Area.ForwardOffset, Direction, Duration);

	ClearExtraTelegraphVFX(ExtraIndex);
	if (UNiagaraSystem* System = Extra.TelegraphVFX.LoadSynchronous())
	{
		if (ExtraTelegraphVFX.Num() <= ExtraIndex)
		{
			ExtraTelegraphVFX.SetNum(ExtraIndex + 1);
		}
		SpawnAreaVFX(Extra.Area, Center, Direction, System, Extra.TelegraphVFXScale, Extra.VFXHeightScale, Extra.VFXPointCount, Extra.VFXRingLayers,
			Extra.VFXReferenceSize, Extra.VFXHeightOffset, Duration, 0.f, &ExtraTelegraphVFX[ExtraIndex].Components);
	}
}

void ATDBossCharacter::MulticastExtraStrike_Implementation(int32 PatternIndex, int32 ExtraIndex, FVector Center, FVector Direction)
{
	if (!Patterns.IsValidIndex(PatternIndex) || !Patterns[PatternIndex].ExtraStrikes.IsValidIndex(ExtraIndex))
	{
		return;
	}
	const FTDBossExtraStrike& Extra = Patterns[PatternIndex].ExtraStrikes[ExtraIndex];

	OnBossEvent.Broadcast(ETDBossEvent::PatternStrike, PatternIndex);
	ClearExtraTelegraphVFX(ExtraIndex);
	if (UNiagaraSystem* System = Extra.StrikeVFX.LoadSynchronous())
	{
		SpawnAreaVFX(Extra.Area, Center, Direction, System, Extra.StrikeVFXScale, Extra.VFXHeightScale, Extra.VFXPointCount, Extra.VFXRingLayers,
			Extra.VFXReferenceSize, Extra.VFXHeightOffset, Extra.StrikeVFXDuration, Extra.StrikeVFXDuration, nullptr);
	}
}

void ATDBossCharacter::MulticastClearTelegraphs_Implementation()
{
	ClearTelegraphVFX();
}

void ATDBossCharacter::ClearTelegraphVFX()
{
	for (UNiagaraComponent* Comp : TelegraphVFXComponents)
	{
		if (Comp != nullptr)
		{
			Comp->DestroyComponent();
		}
	}
	TelegraphVFXComponents.Empty();

	for (int32 Index = 0; Index < ExtraTelegraphVFX.Num(); ++Index)
	{
		ClearExtraTelegraphVFX(Index);
	}
}

void ATDBossCharacter::ClearExtraTelegraphVFX(int32 ExtraIndex)
{
	if (!ExtraTelegraphVFX.IsValidIndex(ExtraIndex))
	{
		return;
	}
	for (UNiagaraComponent* Comp : ExtraTelegraphVFX[ExtraIndex].Components)
	{
		if (Comp != nullptr)
		{
			Comp->DestroyComponent();
		}
	}
	ExtraTelegraphVFX[ExtraIndex].Components.Empty();
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

void ATDBossCharacter::MulticastOneShotVFX_Implementation(UNiagaraSystem* System, FVector Location, float Scale, float KillAfter)
{
	if (System == nullptr || GetWorld() == nullptr)
	{
		return;
	}
	UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Location, FRotator::ZeroRotator,
		FVector(FMath::Max(Scale, 0.01f)), /*bAutoDestroy*/ true, /*bAutoActivate*/ true);
	if (Comp != nullptr && KillAfter > 0.f)
	{
		// 루프 이펙트는 스스로 안 끝난다. 시간이 되면 끄고(남은 입자는 자연 소멸) 자동 파괴에 맡긴다.
		TWeakObjectPtr<UNiagaraComponent> WeakComp = Comp;
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [WeakComp]()
		{
			if (WeakComp.IsValid())
			{
				WeakComp->Deactivate();
			}
		}), KillAfter, false);
	}
}
