#include "Character/Boss2D/TD2DBossCharacter.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

ATD2DBossCharacter::ATD2DBossCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	TeamId = FGenericTeamId(1);

	// 요청한 70% / 40% 페이즈와 2초마다 2마리, 최대 10마리.
	Phase2HealthRatio = 0.7f;
	Phase3HealthRatio = 0.4f;
	// 부모의 1회 소환 대신 이 클래스의 반복 소환만 사용한다.
	bSummonOnPhase2 = false;
	bRepeatSummonFromPhase2 = true;
	SummonInterval = 2.f;
	MinionCount = 2;
	MaxActiveMinions = 10;
	EnrageAfterSeconds = 0.f;
	PatternGapMin = 0.35f;
	PatternGapMax = 0.75f;

	// 페이즈 1~2: 기존 추가 타격 기능으로 불길을 앞쪽으로 순차 전파한다.
	FTDBossPatternSpec Flame;
	Flame.Name = TEXT("FlameLine");
	Flame.Motion = ETDBossMotion::None;
	Flame.Shape = ETDBossHitShape::Box;
	Flame.BoxExtent = FVector(65.f, 110.f, 150.f);
	Flame.BoxDirections = 1;
	Flame.ForwardOffset = 110.f;
	Flame.TelegraphTime = 0.35f;
	Flame.StrikeTime = 0.2f;
	Flame.RecoveryTime = 0.55f;
	Flame.DamageScale = 1.f;
	Flame.Cooldown = 2.f;
	Flame.MinDistance = 0.f;
	Flame.MaxDistance = 1000.f;
	Flame.MinPhase = 1;
	Flame.MaxPhase = 2;
	Flame.Weight = 1.f;
	Flame.ShakeScale = 0.f;
	for (int32 Step = 1; Step <= 6; ++Step)
	{
		FTDBossExtraStrike Segment;
		Segment.Name = FName(*FString::Printf(TEXT("FlameStep%d"), Step));
		Segment.Delay = 0.08f * Step;
		Segment.bCenterOnBoss = false;
		Segment.Area.Shape = ETDBossHitShape::Box;
		Segment.Area.BoxExtent = Flame.BoxExtent;
		Segment.Area.ForwardOffset = Flame.ForwardOffset + 130.f * Step;
		Segment.DamageScale = Flame.DamageScale;
		Segment.ShakeScale = 0.f;
		Flame.ExtraStrikes.Add(Segment);
	}
	Patterns.Add(Flame);

	// 페이즈 3 전용: 대상 위치로 점프 후 착지 원판 + 추적 장판.
	FTDBossPatternSpec Slam;
	Slam.Name = TEXT("JumpSlam");
	Slam.Motion = ETDBossMotion::None;
	Slam.Shape = ETDBossHitShape::Sphere;
	Slam.SphereRadius = 300.f;
	Slam.ForwardOffset = 0.f;
	Slam.TelegraphTime = 0.9f;
	Slam.StrikeTime = 0.15f;
	Slam.RecoveryTime = 0.75f;
	Slam.DamageScale = 1.5f;
	Slam.Cooldown = 7.f;
	Slam.MinDistance = 0.f;
	Slam.MaxDistance = 1400.f;
	Slam.MinPhase = 3;
	Slam.MaxPhase = 3;
	Slam.Weight = 0.8f;
	Slam.ShakeScale = 0.f;
	Slam.Knockback = 500.f;
	Patterns.Add(Slam);

	// 1페이즈에는 후보가 화염 하나뿐이다. 전체 배열 수만 보고 연속 사용을 막으면
	// 2번째 화염부터 후보가 사라지므로 이 보스는 반복 방지를 끈다. 쿨타임이 빈도를 제어한다.
	bAvoidRepeatingPattern = false;
}

void ATD2DBossCharacter::BeginPlay()
{
	// 기존 BP에 true가 저장돼 있어도 공통 보스의 1회 소환과 중복되지 않게 한다.
	bSummonOnPhase2 = false;
	Super::BeginPlay();
	LeashRadius = ArenaRadius;
	// 2D 보스는 텔레그래프/적중 VFX, 카메라 흔들림, 디버그 판정을 사용하지 않는다.
	// 공통 보스의 프로퍼티와 방송 함수는 다른 보스를 위해 그대로 둔다.
	bDrawDebugHits = false;
	CameraShakeClass = nullptr;

	// 이 보스는 별도 텔레그래프 VFX를 사용하지 않는다. 공통 보스 코드는
	// 다른 보스가 계속 사용할 수 있도록 유지하고, 이 보스의 패턴 데이터만 비운다.
	for (FTDBossPatternSpec& Pattern : Patterns)
	{
		Pattern.TelegraphVFX.Reset();
		Pattern.ShakeScale = 0.f;
		for (FTDBossExtraStrike& Extra : Pattern.ExtraStrikes)
		{
			Extra.TelegraphVFX.Reset();
			Extra.ShakeScale = 0.f;
		}
	}

	OnBossEvent.AddDynamic(this, &ATD2DBossCharacter::HandleBossVisualEvent);
	OnPatternTelegraph.AddDynamic(this, &ATD2DBossCharacter::HandlePatternVisual);
	OnDamaged.AddDynamic(this, &ATD2DBossCharacter::HandleDamagedVisual);
	OnRespawn.AddDynamic(this, &ATD2DBossCharacter::HandleRespawnVisual);
	ArenaPatternFinishedHandle = OnPatternFinished.AddUObject(this, &ATD2DBossCharacter::HandleArenaPatternFinished);
	if (HasAuthority())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			ArenaHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UTDAttributeSet::GetHealthAttribute())
				.AddUObject(this, &ATD2DBossCharacter::HandleArenaHealthChanged);
		}
	}
	RefreshFlipbook();
}

void ATD2DBossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearArenaFight();
	OnPatternFinished.Remove(ArenaPatternFinishedHandle);
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UTDAttributeSet::GetHealthAttribute()).Remove(ArenaHealthChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ATD2DBossCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshFlipbook();

	if (!HasAuthority() || IsDead())
	{
		return;
	}
	// 부모 포인터로 리셋된 경우에도 전용 타이머 / 쫄몹은 다음 틱에 정리된다.
	if (!IsFightActive() && (bPhase3Transition || !ArenaMinions.IsEmpty()
		|| GetWorldTimerManager().IsTimerActive(ArenaSummonTimerHandle)))
	{
		ClearArenaFight();
	}

	if (bJumping)
	{
		TickJumpSlam(DeltaSeconds);
	}

	// 목적지가 안쪽이어도 NavMesh 경로가 원 밖으로 휠 수 있다. 최종 안전망으로
	// 실제 위치도 매 프레임 원 안에 둔다(점프 XY도 같은 규칙을 적용받는다).
	const FVector Clamped = ClampLocationToArena(GetActorLocation());
	if (!Clamped.Equals(GetActorLocation(), 0.1f))
	{
		SetActorLocation(Clamped, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

bool ATD2DBossCharacter::IsLocationInsideArena(const FVector& Location) const
{
	if (ArenaRadius <= 0.f)
	{
		return false;
	}

	// 경기장 중심은 현재 보스 위치가 아니라 공통 보스가 BeginPlay에서 저장한
	// HomeLocation(배치/스폰 위치)이다. 따라서 보스가 움직여도 원 자체는 따라오지 않는다.
	// Z(높이)는 무시하고 XY 평면 거리만 비교한다.
	return FVector::DistSquared2D(Location, GetHomeLocation()) <= FMath::Square(ArenaRadius);
}

FVector ATD2DBossCharacter::ClampLocationToArena(const FVector& Location, float ExtraMargin) const
{
	// 실제 이동 제한은 캡슐과 경계 여백을 고려한 안전 반경으로 고정 중심(HomeLocation)에
	// 맞춘다. 점프 중에도 XY 위치가 이 안전 원을 벗어나지 않도록 Tick에서 호출된다.
	const float CapsuleRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
	const float SafeRadius = FMath::Max(0.f, ArenaRadius - ArenaBoundaryMargin - CapsuleRadius - ExtraMargin);

	FVector FlatDelta = Location - GetHomeLocation();
	FlatDelta.Z = 0.f;
	if (FlatDelta.SizeSquared() <= FMath::Square(SafeRadius))
	{
		return Location;
	}

	const FVector Flat = GetHomeLocation() + FlatDelta.GetSafeNormal() * SafeRadius;
	return FVector(Flat.X, Flat.Y, Location.Z);
}

ATDCharacterBase* ATD2DBossCharacter::FindNearestEnemyInArena() const
{
	// 후보가 고정된 경기장 원 안에 있는지 먼저 확인한 뒤, 현재 보스 위치에서 가장 가까운
	// 적을 고른다. 즉, 감지 영역의 중심은 HomeLocation이고 거리 우선순위만 현재 위치 기준이다.
	ATDCharacterBase* Best = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();

	for (TActorIterator<ATDCharacterBase> It(GetWorld()); It; ++It)
	{
		ATDCharacterBase* Candidate = *It;
		if (Candidate == nullptr || Candidate == this || Candidate->IsDead()
			|| Candidate->GetGenericTeamId() == GetGenericTeamId()
			|| !IsLocationInsideArena(Candidate->GetActorLocation()))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Candidate;
		}
	}
	return Best;
}

void ATD2DBossCharacter::ResetArenaFight(bool bInstant)
{
	if (!HasAuthority() || IsDead()) return;
	ClearArenaFight();
	Super::ResetFight(bInstant);
}

void ATD2DBossCharacter::HandleDeath()
{
	if (IsDead())
	{
		return;
	}

	ClearArenaFight();
	Super::HandleDeath();

	if (HasAuthority())
	{
		// 공통 몬스터의 사망 처리는 유지하고, 이 보스의 파괴 예약 시간만 덮어쓴다.
		// 서버도 플립북 길이로 계산하므로 렌더링하지 않는 데디 서버에서 동작한다.
		const UPaperFlipbook* DeathFlipbook = PickDirectionalFlipbook(DieAnimation);
		const float PlayRate = GetSpriteComponent() ? FMath::Abs(GetSpriteComponent()->GetPlayRate()) : 1.f;
		const float AnimationDuration = DeathFlipbook && PlayRate > KINDA_SMALL_NUMBER
			? DeathFlipbook->GetTotalDuration() / PlayRate : 0.f;
		// SetLifeSpan(0)은 즉시 파괴가 아니라 예약 취소이므로 최소 양수를 보장한다.
		SetLifeSpan(FMath::Max(AnimationDuration + FMath::Max(DeathHoldDuration, 0.f), KINDA_SMALL_NUMBER));
	}
}

bool ATD2DBossCharacter::IsInvulnerable() const
{
	return bJumping || bPhase3Transition || Super::IsInvulnerable();
}

void ATD2DBossCharacter::OnTelegraphBegin(const FTDBossPatternSpec& Spec)
{
	Super::OnTelegraphBegin(Spec);
	if (IsJumpSlamPattern(Spec))
	{
		StartJumpSlam(FMath::Max(ScaledTelegraph(Spec.TelegraphTime), 0.01f));
	}
}

void ATD2DBossCharacter::OnMotionBegin(const FTDBossPatternSpec& Spec)
{
	if (IsJumpSlamPattern(Spec))
	{
		FinishJumpSlam();
	}
	Super::OnMotionBegin(Spec);

	// 기본공격은 공통 보스의 본 타격 직전에 한 번만 방송한다.
	// FlameLine의 시간차 ExtraStrikes에는 다시 방송하지 않으므로 소리가 연속 중첩되지 않는다.
	if (Phase <= 2 && Spec.Name == BasicAttackPatternName && BasicAttackSound != nullptr)
	{
		const FVector SoundLocation = GetStrikeBase(Spec)
			+ GetFacing() * Spec.ForwardOffset;
		Multicast2DBossAction(ETD2DBossAction::BasicAttack, SoundLocation);
	}
	// 내려찍기는 부모의 DoStrikeHit() 1회 판정만 사용한다.
	// 반복 장판은 더 이상 시작하지 않는다.
}

void ATD2DBossCharacter::OnMotionEnd(const FTDBossPatternSpec& Spec)
{
	Super::OnMotionEnd(Spec);
	if (IsJumpSlamPattern(Spec))
	{
		SetMovementFrozen(false);
	}
}

void ATD2DBossCharacter::HandleArenaPatternFinished(int32 PatternIndex)
{
	AbortJumpSlam();
}

FVector ATD2DBossCharacter::GetStrikeBase(const FTDBossPatternSpec& Spec) const
{
	if (!IsJumpSlamPattern(Spec))
	{
		return Super::GetStrikeBase(Spec);
	}

	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	// 부모는 OnTelegraphBegin 이전에 예고 위치를 묻는다. 같은 타깃으로 미리 계산하면
	// 공통 StartPattern 순서를 바꾸지 않고도 실제 착지점에 예고를 띄울 수 있다.
	if (CurrentPatternPhase == ETDBossPatternPhase::Telegraph && !bJumping)
	{
		FVector Landing = GetPatternTarget() ? GetPatternTarget()->GetActorLocation() : GetActorLocation();
		Landing.Z = GetActorLocation().Z;
		Landing = ClampLocationToArena(Landing);
		return Landing - FVector(0.f, 0.f, HalfHeight);
	}
	return FVector(JumpTo.X, JumpTo.Y, JumpTo.Z - HalfHeight);
}

void ATD2DBossCharacter::HandleArenaHealthChanged(const FOnAttributeChangeData& Data)
{
	if (!HasAuthority() || IsDead() || Data.NewValue <= 0.f || Phase >= 3) return;

	// AI 서비스가 아직 다음 0.2초 틱을 돌기 전에도 경기장 안의 첫 공격은
	// 정상적인 전투 시작으로 취급한다. 이 보정이 없으면 첫 큰 타격이 40% 아래로
	// 바로 내려갈 때 부모의 2페이즈만 들어가고 3페이즈 전환은 놓칠 수 있다.
	if (!IsFightActive())
	{
		if (ATDCharacterBase* Target = FindNearestEnemyInArena())
		{
			BeginFight(Target);
		}
		else
		{
			return;
		}
	}

	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	const float MaxHealth = ASC ? ASC->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute()) : 0.f;
	if (MaxHealth > 0.f && Phase3HealthRatio > 0.f && Data.NewValue / MaxHealth <= Phase3HealthRatio)
	{
		// 한 번의 큰 피해로 두 경계를 넘겨도 기존 2페이즈 진입은 그대로 실행한다.
		if (Phase < 2) EnterPhase2();
		EnterArenaPhase3();
	}
}

void ATD2DBossCharacter::EnterArenaPhase3()
{
	if (!HasAuthority() || Phase >= 3 || IsDead()) return;
	bPhase3Transition = true;
	CancelPattern();
	// 기존 IsBusy/StartPattern도 전환 중임을 알도록, 패턴 번호가 없는 Recovery로 잠근다.
	// 새 virtual API 없이 부모가 이미 사용하는 패턴 상태를 통해 진입을 차단한다.
	CurrentPatternPhase = ETDBossPatternPhase::Recovery;
	Phase = 3; // 부모가 원래 제공하는 protected 상태 / 복제 / 조회를 재사용한다.
	OnPhaseChanged.Broadcast(Phase);
	ForceNetUpdate();
	Multicast2DBossAction(ETD2DBossAction::Phase3Started, GetActorLocation());
	GetWorldTimerManager().SetTimer(ArenaPhase3TimerHandle, this,
		&ATD2DBossCharacter::FinishArenaPhase3Transition, FMath::Max(Phase3TransitionDuration, 0.01f), false);
}

void ATD2DBossCharacter::FinishArenaPhase3Transition()
{
	bPhase3Transition = false;
	if (CurrentPattern == INDEX_NONE && CurrentPatternPhase == ETDBossPatternPhase::Recovery)
	{
		CurrentPatternPhase = ETDBossPatternPhase::None;
	}
}

void ATD2DBossCharacter::StartArenaSummons()
{
	if (!HasAuthority() || !bRepeatSummonFromPhase2 || !IsFightActive() || IsDead()) return;
	if (GetWorldTimerManager().IsTimerActive(ArenaSummonTimerHandle)) return;
	SummonArenaMinions();
	GetWorldTimerManager().SetTimer(ArenaSummonTimerHandle, this, &ATD2DBossCharacter::SummonArenaMinions,
		FMath::Max(SummonInterval, 0.1f), true);
}

void ATD2DBossCharacter::SummonArenaMinions()
{
	if (!HasAuthority() || !IsFightActive() || IsDead() || Phase < 2 || !MinionClass) return;
	ArenaMinions.RemoveAll([](const TWeakObjectPtr<ATDEnemyBase>& Minion) { return !Minion.IsValid() || Minion->IsDead(); });
	const int32 SpawnCount = FMath::Min(MinionCount, FMath::Max(0, MaxActiveMinions - ArenaMinions.Num()));
	const ATDEnemyBase* Defaults = MinionClass->GetDefaultObject<ATDEnemyBase>();
	const float HalfHeight = Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Radius = Defaults->GetCapsuleComponent()->GetScaledCapsuleRadius();
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	int32 Spawned = 0;
	for (int32 Index = 0; Index < SpawnCount; ++Index)
	{
		const float Angle = 2.f * PI * Index / SpawnCount;
		FVector Location = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * MinionSpawnRadius;
		Location = ClampLocationToArena(Location, Radius);
		Location.Z = bJumping ? JumpFrom.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + HalfHeight : GetFloorZ() + HalfHeight;
		ATDEnemyBase* Minion = GetWorld()->SpawnActor<ATDEnemyBase>(MinionClass, Location, GetActorRotation(), Params);
		if (!Minion) continue;
		Minion->SetActorLocation(ClampLocationToArena(Minion->GetActorLocation(), Radius));
		if (!MinionId.IsNone()) Minion->InitializeFromDefinition(MinionId, GetLevel());
		ArenaMinions.Add(Minion);
		++Spawned;
	}
	if (Spawned > 0) MulticastBossEvent(ETDBossEvent::Summon, Spawned);
}

void ATD2DBossCharacter::ClearArenaFight()
{
	AbortJumpSlam();
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(ArenaSummonTimerHandle);
		GetWorldTimerManager().ClearTimer(ArenaPhase3TimerHandle);
	}
	bPhase3Transition = false;
	for (const TWeakObjectPtr<ATDEnemyBase>& Minion : ArenaMinions)
	{
		if (Minion.IsValid()) Minion->Destroy();
	}
	ArenaMinions.Empty();
}

void ATD2DBossCharacter::StartJumpSlam(float Duration)
{
	AbortJumpSlam();
	JumpFrom = GetActorLocation();
	JumpTo = GetPatternTarget() ? GetPatternTarget()->GetActorLocation() : JumpFrom;
	JumpTo.Z = JumpFrom.Z;
	JumpTo = ClampLocationToArena(JumpTo);
	JumpElapsed = 0.f;
	JumpDuration = FMath::Max(Duration, 0.01f);
	bJumping = true;
	SetMovementFrozen(true);
	Multicast2DBossAction(ETD2DBossAction::JumpStarted, JumpFrom);
}

void ATD2DBossCharacter::TickJumpSlam(float DeltaSeconds)
{
	JumpElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(JumpElapsed / JumpDuration, 0.f, 1.f);
	FVector Next = FMath::Lerp(JumpFrom, JumpTo, Alpha);
	Next.Z += FMath::Sin(Alpha * PI) * JumpHeight;
	SetActorLocation(Next, false, nullptr, ETeleportType::TeleportPhysics);
}

void ATD2DBossCharacter::FinishJumpSlam()
{
	if (!bJumping)
	{
		return;
	}
	bJumping = false;
	SetActorLocation(JumpTo, false, nullptr, ETeleportType::TeleportPhysics);
	Multicast2DBossAction(ETD2DBossAction::Landed, JumpTo);
}

void ATD2DBossCharacter::AbortJumpSlam()
{
	if (!bJumping)
	{
		return;
	}
	bJumping = false;
	FVector Grounded = GetActorLocation();
	Grounded.Z = JumpFrom.Z;
	SetActorLocation(ClampLocationToArena(Grounded), false, nullptr, ETeleportType::TeleportPhysics);
	SetMovementFrozen(false);
}

void ATD2DBossCharacter::Multicast2DBossAction_Implementation(ETD2DBossAction Action, FVector Location)
{
	On2DBossAction.Broadcast(Action);

	switch (Action)
	{
	case ETD2DBossAction::JumpStarted:
		VisualAction = EVisualAction::Jump;
		RefreshFlipbook();
		break;

	case ETD2DBossAction::Landed:
		VisualAction = EVisualAction::Attack;
		RefreshFlipbook();
		// 착지 순간에 임팩트 VFX와 사운드를 재생한다. 실제 1회 데미지는
		// 같은 EnterStrike 흐름의 부모 DoStrikeHit()에서 바로 처리된다.
		PlayLandingVFX(Location);
		if (LandingSound)
		{
			if (GetNetMode() != NM_DedicatedServer)
			{
				UGameplayStatics::PlaySoundAtLocation(this, LandingSound, Location);
			}
		}
		break;

	case ETD2DBossAction::BasicAttack:
		// 데디케이티드 서버는 오디오 장치가 없으므로 클라이언트에서만 재생한다.
		if (BasicAttackSound != nullptr && GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::PlaySoundAtLocation(this, BasicAttackSound, Location);
		}
		break;

	case ETD2DBossAction::Phase3Started:
		VisualAction = EVisualAction::None;
		RefreshFlipbook();
		break;
	}
}

void ATD2DBossCharacter::PlayLandingVFX(FVector Location)
{
	UNiagaraSystem* ImpactVFX = LandingVFX.Get();
	if (ImpactVFX && GetNetMode() != NM_DedicatedServer)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactVFX, Location);
	}
}

void ATD2DBossCharacter::HandleBossVisualEvent(ETDBossEvent Event, int32 Param)
{
	switch (Event)
	{
	case ETDBossEvent::PatternTelegraph:
		if (Patterns.IsValidIndex(Param))
		{
			VisualAction = IsJumpSlamPattern(Patterns[Param])
				? EVisualAction::Jump
				: EVisualAction::Attack;
		}
		break;

	case ETDBossEvent::PatternStrike:
		VisualAction = EVisualAction::Attack;
		break;

	case ETDBossEvent::Reset:
		ClearArenaFight();
		VisualAction = EVisualAction::None;
		bVisualDead = false;
		break;
	case ETDBossEvent::Phase2:
		StartArenaSummons();
		break;
	case ETDBossEvent::PatternEnd:
		VisualAction = EVisualAction::None;
		bVisualDead = false;
		break;

	case ETDBossEvent::Death:
		VisualAction = EVisualAction::None;
		bVisualDead = true;
		break;

	default:
		break;
	}
	RefreshFlipbook();
}

void ATD2DBossCharacter::HandlePatternVisual(int32 PatternIndex, FVector Center, FVector Direction, float Duration)
{
	if (!Direction.IsNearlyZero())
	{
		VisualFacing = Direction.GetSafeNormal2D();
	}
	if (Patterns.IsValidIndex(PatternIndex))
	{
		VisualAction = IsJumpSlamPattern(Patterns[PatternIndex])
			? EVisualAction::Jump
			: EVisualAction::Attack;
	}
	RefreshFlipbook();
}

void ATD2DBossCharacter::HandleDamagedVisual(AActor* Attacker, float Damage, bool bCritical)
{
	if (!bVisualDead && !IsDead() && GetWorld())
	{
		HitVisualEndTime = GetWorld()->GetTimeSeconds() + HitAnimationDuration;
		RefreshFlipbook();
	}
}

void ATD2DBossCharacter::HandleRespawnVisual()
{
	bVisualDead = false;
	VisualAction = EVisualAction::None;
	HitVisualEndTime = -1.f;
	RefreshFlipbook();
}

UPaperFlipbook* ATD2DBossCharacter::PickDirectionalFlipbook(const FTD2DDirectionalFlipbooks& Set) const
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

void ATD2DBossCharacter::RefreshFlipbook()
{
	const FVector Velocity2D(GetVelocity().X, GetVelocity().Y, 0.f);
	// 사망 방향은 고정한다. 서버도 같은 방향을 추적해 해당 Die 길이로 수명을 계산한다.
	if (!bVisualDead && !IsDead() && VisualAction == EVisualAction::None && Velocity2D.SizeSquared() > FMath::Square(5.f))
	{
		VisualFacing = Velocity2D.GetSafeNormal();
	}

	if (GetNetMode() == NM_DedicatedServer || GetSpriteComponent() == nullptr)
	{
		return;
	}

	const bool bHit = GetWorld() && GetWorld()->GetTimeSeconds() < HitVisualEndTime;
	const bool bMoving = Velocity2D.SizeSquared() > FMath::Square(5.f);
	const FTD2DDirectionalFlipbooks* Set = &IdleAnimation;
	bool bLoop = true;

	if (bVisualDead || IsDead())
	{
		Set = &DieAnimation;
		bLoop = false;
	}
	else if (bHit)
	{
		Set = &HitAnimation;
		bLoop = false;
	}
	else if (VisualAction == EVisualAction::Jump)
	{
		Set = &JumpAnimation;
		bLoop = false;
	}
	else if (VisualAction == EVisualAction::Attack)
	{
		Set = &AttackAnimation;
		bLoop = false;
	}
	else if (bMoving)
	{
		Set = &WalkAnimation;
	}

	UPaperFlipbook* Desired = PickDirectionalFlipbook(*Set);
	if (Desired == nullptr || Desired == CurrentVisualFlipbook)
	{
		return;
	}

	CurrentVisualFlipbook = Desired;
	GetSpriteComponent()->SetFlipbook(Desired);
	GetSpriteComponent()->SetLooping(bLoop);
	GetSpriteComponent()->PlayFromStart();
}
