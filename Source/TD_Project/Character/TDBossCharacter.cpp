#include "Character/TDBossCharacter.h"

#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Camera/CameraShakeBase.h"
#include "Combat/TDCombatComponent.h"
#include "Combat/TDCombatStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ATDBossCharacter::ATDBossCharacter()
{
	// 보스는 경직이 없다(CanBeStaggered=false). 값은 남겨두되 의미 없음.
	HitStaggerDuration = 0.f;
	// 사망 연출(가라앉기)을 볼 시간. BP 에서 조절.
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

// ── 조회 ─────────────────────────────────────────────────

bool ATDBossCharacter::IsBusy() const
{
	return CurrentPatternPhase != ETDBossPatternPhase::None || bInEntrance || bInPhaseTransition;
}

bool ATDBossCharacter::IsInvulnerable() const
{
	return bInEntrance || bInPhaseTransition;
}

float ATDBossCharacter::GetIncomingDamageMultiplier() const
{
	return CurrentPatternPhase == ETDBossPatternPhase::Recovery ? RecoveryIncomingDamageMultiplier : 1.f;
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
	if (!HasAuthority())
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

	SetActorLocation(HomeLocation);
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
		PatternTargetLocation = GetStrikeCenter(Spec);
	}

	CurrentPattern = PatternIndex;
	CurrentPatternPhase = ETDBossPatternPhase::Telegraph;
	MulticastBossEvent(ETDBossEvent::PatternTelegraph, PatternIndex);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugHits)
	{
		// 서버 화면용 예고. 클라 예고 표시는 BP(OnBossEvent) 몫.
		const FVector Center = GetStrikeCenter(Spec);
		const float Duration = ScaledTelegraph(Spec.TelegraphTime);
		if (Spec.Shape == ETDBossHitShape::Box)
		{
			const FQuat Rot = FRotationMatrix::MakeFromX(GetCombatComponent()->GetFacingDirection()).ToQuat();
			DrawDebugBox(GetWorld(), Center, Spec.BoxExtent, Rot, FColor::Yellow, false, Duration);
		}
		else
		{
			DrawDebugSphere(GetWorld(), Center, Spec.SphereRadius, 24, FColor::Yellow, false, Duration);
		}
	}
#endif

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATDBossCharacter::EnterStrike,
		FMath::Max(ScaledTelegraph(Spec.TelegraphTime), 0.01f), false);
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

	OnMotionBegin(Spec);   // 2단계: 돌진 시작 / 솟구침 / 투사체 발사

	// 판정: 한 번, 또는 StrikeTime 동안 간격 반복.
	DoStrikeHit();
	if (Spec.StrikeInterval > 0.f && Spec.StrikeTime > Spec.StrikeInterval)
	{
		GetWorldTimerManager().SetTimer(StrikeTickHandle, this, &ATDBossCharacter::DoStrikeHit,
			Spec.StrikeInterval, true);
	}

	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATDBossCharacter::EnterRecovery,
		FMath::Max(Spec.StrikeTime, 0.01f), false);
}

FVector ATDBossCharacter::GetStrikeCenter(const FTDBossPatternSpec& Spec) const
{
	const FVector Facing = GetCombatComponent() ? GetCombatComponent()->GetFacingDirection() : GetActorForwardVector();
	return GetActorLocation() + Facing * Spec.ForwardOffset;
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
		const FQuat Rot = FRotationMatrix::MakeFromX(GetCombatComponent()->GetFacingDirection()).ToQuat();
		Enemies = UTDCombatStatics::GatherEnemiesInBox(this, Center, Rot, Spec.BoxExtent, bDrawDebugHits);
	}
	else
	{
		Enemies = UTDCombatStatics::GatherEnemiesInSphere(this, Center, Spec.SphereRadius, bDrawDebugHits);
	}

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
	// 페이즈 전환·사망·리셋이 패턴을 끊을 때. 쿨은 안 건다.
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
	GetWorldTimerManager().ClearTimer(StrikeTickHandle);

	if (CurrentPatternPhase == ETDBossPatternPhase::None)
	{
		return;
	}
	if (Patterns.IsValidIndex(CurrentPattern) && CurrentPatternPhase == ETDBossPatternPhase::Strike)
	{
		OnMotionEnd(Patterns[CurrentPattern]);
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

// ── 페이즈·분노 ───────────────────────────────────────────

void ATDBossCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (!HasAuthority() || Phase >= 2 || IsDead())
	{
		return;
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

	int32 Spawned = 0;
	for (int32 Index = 0; Index < MinionCount; ++Index)
	{
		// 보스 주변 원 위에 고르게. 겹치면 스폰 파라미터가 옆으로 밀어낸다.
		const float Angle = (2.f * PI / MinionCount) * Index;
		const FVector Offset(FMath::Cos(Angle) * MinionSpawnRadius, FMath::Sin(Angle) * MinionSpawnRadius, 0.f);

		ATDEnemyBase* Minion = GetWorld()->SpawnActor<ATDEnemyBase>(
			MinionClass, GetActorLocation() + Offset, GetActorRotation(), Params);
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
	CancelPattern();
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

	// 이 머신의 로컬 플레이어만. 거리 감쇠는 여기서 직접 계산한다.
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