#include "Skill/TDSkillComponent.h"

#include "Character/TDCharacterBase.h"
#include "Components/AudioComponent.h"
#include "Combat/TDCombatComponent.h"
#include "Combat/TDCombatStatics.h"
#include "Components/CapsuleComponent.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDSkillEffectRow.h"
#include "Data/TDSkillRow.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TDInteractionFlowComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Stats/TDProgressionComponent.h"
#include "Sound/SoundBase.h"
#include "Stats/TDStatComponent.h"
#include "TimerManager.h"

namespace
{
	/**
	 * 스킬 상자의 반높이(cm). 평타의 HitBoxExtent.Z 와 같은 값이다.
	 *
	 * DT_Skill 에 열로 두지 않은 이유는 2.5D 라 모두 같은 평면에 있어서다.
	 * 공중 판정이 필요해지면 그때 열을 더한다.
	 */
	constexpr float SkillBoxHalfHeight = 80.f;

#if !UE_BUILD_SHIPPING
	int32 GTDIgnoreSkillCooldown = 0;

	FAutoConsoleVariableRef CVarIgnoreSkillCooldown(
		TEXT("TD.NoCooldown"),
		GTDIgnoreSkillCooldown,
		TEXT("1이면 스킬 쿨타임을 무시한다. 검사도 기록도 하지 않는다.\n")
		TEXT("판정은 서버에서 하므로 **서버 창(또는 단일 PIE)에서** 켤 것."));
#endif

	/** 쿨타임을 건너뛰는 중인가. 배포 빌드에서는 언제나 false 이므로 분기가 사라진다. */
	bool IsSkillCooldownIgnored()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return GTDIgnoreSkillCooldown != 0;
#endif
	}
}

UTDSkillComponent::UTDSkillComponent()
{
	// 시전 중에만 돈다. 이동 감시 말고는 매 프레임 할 일이 없다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Multicast RPC 가 클라이언트까지 가려면 컴포넌트 자신도 복제 대상이어야 한다.
	SetIsReplicatedByDefault(true);
}

void UTDSkillComponent::BeginPlay()
{
	Super::BeginPlay();

	// 경직이 평타를 끊는 것과 같은 규칙으로 시전도 끊는다.
	// OnDamagedServer 는 서버에서만 발화하므로 권한 검사를 따로 하지 않아도 된다.
	if (ATDCharacterBase* Owner = GetOwnerCharacter())
	{
		DamagedHandle = Owner->OnDamagedServer.AddUObject(this, &UTDSkillComponent::HandleDamaged);
	}
}

void UTDSkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ATDCharacterBase* Owner = GetOwnerCharacter())
	{
		Owner->OnDamagedServer.Remove(DamagedHandle);
	}

	// 캐릭터가 사라지는데 장판 이펙트만 남으면 안 된다.
	StopChannelVFX();
	StopChannelSFX();

	Super::EndPlay(EndPlayReason);
}

void UTDSkillComponent::HandleDamaged(AActor* Attacker, float Damage, bool bCritical)
{
	// 경직이 걸리지 않는 타격(HitStaggerDuration 이 0)은 시전을 끊지 않는다.
	// 스치기만 해도 캐스팅이 날아가면 전투가 답답해진다.
	const ATDCharacterBase* Owner = GetOwnerCharacter();
	if (Owner != nullptr && Owner->IsStaggered())
	{
		CancelCast();
	}
}

// ── 조회 ──────────────────────────────────────────────────

ATDCharacterBase* UTDSkillComponent::GetOwnerCharacter() const
{
	return Cast<ATDCharacterBase>(GetOwner());
}

UTDProgressionComponent* UTDSkillComponent::GetProgression() const
{
	const ATDCharacterBase* Owner = GetOwnerCharacter();
	return Owner ? Owner->GetProgressionComponent() : nullptr;
}

const FTDSkillRow* UTDSkillComponent::GetCastingRow() const
{
	const UTDProgressionComponent* Progression = GetProgression();
	return Progression ? Progression->FindSkillRow(CastingSkillId) : nullptr;
}

ETDSkillCastMovement UTDSkillComponent::GetCastMovement() const
{
	const FTDSkillRow* Row = GetCastingRow();
	return Row ? Row->CastMovement : ETDSkillCastMovement::Free;
}

float UTDSkillComponent::GetCooldownRemaining(FName SkillId) const
{
	const float* EndTime = CooldownEndTimes.Find(SkillId);
	const UWorld* World = GetWorld();
	if (EndTime == nullptr || World == nullptr)
	{
		return 0.f;
	}

	return FMath::Max(0.f, *EndTime - World->GetTimeSeconds());
}

float UTDSkillComponent::GetCooldownRemainingForSlot(int32 SlotIndex) const
{
	const UTDProgressionComponent* Progression = GetProgression();
	return Progression ? GetCooldownRemaining(Progression->GetSkillForSlot(SlotIndex)) : 0.f;
}

float UTDSkillComponent::GetManaCost(const FTDSkillRow& Row, int32 SkillLevel) const
{
	return FMath::Max(0.f, Row.ManaCost + Row.ManaCostPerLevel * (SkillLevel - 1));
}

float UTDSkillComponent::GetActualCooldown(const FTDSkillRow& Row, int32 SkillLevel) const
{
	// 레벨을 따라 줄어드는 쿨(CooldownPerLevel 이 음수)이 있다. 0 아래로는 내려가지 않는다.
	const float BaseCooldown =
		FMath::Max(0.f, Row.Cooldown + Row.CooldownPerLevel * (SkillLevel - 1));

	const ATDCharacterBase* Owner = GetOwnerCharacter();
	if (Owner == nullptr)
	{
		return BaseCooldown;
	}

	// 실제 쿨타임 = 기본 ÷ (1 + 회복률). 평타(UTDCombatComponent::CanAttack)와 같은 규칙이다.
	const float Recovery = FMath::Max(0.f, Owner->GetStat(TDTags::Stat_Utility_CooldownRecoveryRate));
	return BaseCooldown / (1.f + Recovery);
}

// ── 시전 시작 ─────────────────────────────────────────────

void UTDSkillComponent::ServerUseSkillSlot_Implementation(int32 SlotIndex)
{
	UTDProgressionComponent* Progression = GetProgression();
	if (Progression == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("스킬 %d: ProgressionComponent 가 없다. 캐릭터를 먼저 선택할 것."), SlotIndex);
		return;
	}

	const FName SkillId = Progression->GetSkillForSlot(SlotIndex);
	if (SkillId.IsNone())
	{
		// 그 자리에 스킬이 없는 직업이다. 오류로 띄울 일은 아니다.
		UE_LOG(LogTemp, Verbose, TEXT("스킬 %d: 이 직업에는 그 자리의 스킬이 없다."), SlotIndex);
		return;
	}

	const FTDSkillRow* Row = Progression->FindSkillRow(SkillId);
	if (Row == nullptr || !CanStartCast(SkillId, *Row))
	{
		return;
	}

	bCostPaid = false;

	// 전 머신에 알린다. CastingSkillId 는 이 안에서 세팅되므로 서버도 같은 경로를 탄다.
	MulticastOnCastStarted(SkillId, Row->CastDelay, Row->CastType);

	// 이동 감시는 시전 중에만 돈다.
	SetComponentTickEnabled(true);

	if (Row->CastDelay > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			CastTimerHandle, this, &UTDSkillComponent::OnCastFinished, Row->CastDelay, false);
	}
	else
	{
		OnCastFinished();
	}
}

bool UTDSkillComponent::CanStartCast(FName SkillId, const FTDSkillRow& Row) const
{
	const ATDCharacterBase* Owner = GetOwnerCharacter();
	const UTDProgressionComponent* Progression = GetProgression();
	if (Owner == nullptr || Progression == nullptr)
	{
		return false;
	}

	if (Owner->IsDead())
	{
		return false;
	}

	// 경직 중엔 시전을 시작할 수 없다. 평타(CanAttack)와 같은 규칙이다 —
	// 한쪽만 막으면 맞았을 때 평타는 멈추고 스킬만 나가는 상태가 된다.
	if (Owner->IsStaggered())
	{
		return false;
	}

	// 이미 시전 중이면 무시한다. 앞의 것을 끊고 새로 시작하지 않는 이유는,
	// 연타로 캐스팅을 계속 리셋하는 쪽이 플레이어에게 더 답답하기 때문이다.
	if (IsCasting())
	{
		return false;
	}

	// 대화·컷씬 중에는 못 쓴다. 평타와 같은 규칙이다.
	const APlayerController* Controller = Cast<APlayerController>(Owner->GetController());
	const UTDInteractionFlowComponent* Flow =
		Controller ? Controller->FindComponentByClass<UTDInteractionFlowComponent>() : nullptr;
	if (Flow != nullptr && Flow->IsGameplayLocked())
	{
		return false;
	}

	if (Row.SkillType != ETDSkillType::Active)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("스킬 '%s' 는 패시브다. DT_Skill 의 SlotIndex 를 확인할 것."), *SkillId.ToString());
		return false;
	}

	const int32 SkillLevel = Progression->GetSkillLevel(SkillId);
	if (SkillLevel <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("스킬 '%s' 를 아직 찍지 않았다."), *SkillId.ToString());
		return false;
	}

	if (!IsSkillCooldownIgnored() && GetCooldownRemaining(SkillId) > 0.f)
	{
		return false;
	}

	// 여기서는 검사만 한다. 실제 소모는 첫 판정 때다 —
	// 캐스팅 중에 끊기면 마나를 되돌릴 필요 자체가 없어진다.
	if (UTDCombatStatics::GetMana(Owner) < GetManaCost(Row, SkillLevel))
	{
		UE_LOG(LogTemp, Log, TEXT("스킬 '%s': 마나가 부족하다."), *SkillId.ToString());
		return false;
	}

	return true;
}

// ── 판정 ──────────────────────────────────────────────────

void UTDSkillComponent::OnCastFinished()
{
	const FTDSkillRow* Row = GetCastingRow();
	if (Row == nullptr)
	{
		EndCast(false);
		return;
	}

	// 값을 치르고 첫 판정을 낸다. 실패하면 FireOnce 안에서 EndCast 가 불린다.
	FireOnce();

	if (!IsCasting())
	{
		return;
	}

	const bool bIsChannel = Row->CastType == ETDSkillCastType::Channel
		&& Row->ChannelDuration > 0.f
		&& Row->ChannelInterval > 0.f;

	if (!bIsChannel)
	{
		EndCast(bCostPaid);
		return;
	}

	// 첫 판정은 방금 냈으므로 다음 판정은 한 간격 뒤다.
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	Timers.SetTimer(ChannelTickHandle, this, &UTDSkillComponent::FireOnce,
		Row->ChannelInterval, /*bLoop=*/true, /*FirstDelay=*/Row->ChannelInterval);

	// 지속시간이 끝나면 반복을 멈춘다. 판정 하나가 한 간격을 대표하므로
	// 마지막 판정 뒤에도 한 간격만큼 정신집중 상태가 유지된다.
	Timers.SetTimer(ChannelEndHandle,
		FTimerDelegate::CreateUObject(this, &UTDSkillComponent::EndCast, true),
		Row->ChannelDuration, false);
}

void UTDSkillComponent::FireOnce()
{
	ATDCharacterBase* Owner = GetOwnerCharacter();
	UTDProgressionComponent* Progression = GetProgression();
	const FTDSkillRow* Row = GetCastingRow();

	if (Owner == nullptr || Progression == nullptr || Row == nullptr || Owner->IsDead())
	{
		EndCast(bCostPaid);
		return;
	}

	const int32 SkillLevel = Progression->GetSkillLevel(CastingSkillId);

	// 첫 판정에서만 마나를 치른다. 정신집중 중에는 다시 걷지 않는다 —
	// 매 틱 걷으면 도중에 떨어졌을 때를 처리해야 하고 서버가 매번 검증해야 한다.
	if (!bCostPaid)
	{
		if (!UTDCombatStatics::ConsumeMana(Owner, GetManaCost(*Row, SkillLevel)))
		{
			// 시작 검사 뒤에 마나가 줄어든 경우다(포션이 아니라 다른 소모원).
			UE_LOG(LogTemp, Log,
				TEXT("스킬 '%s': 발동 시점에 마나가 모자라 끊겼다."), *CastingSkillId.ToString());
			EndCast(false);
			return;
		}

		bCostPaid = true;

		// 이펙트는 첫 판정에서 한 번만 띄운다. 시전 시작에 띄우면 캐스팅이 끝나기 전에
		// 먼저 터지고, 이동으로 끊긴 캐스팅도 이펙트는 나가 버린다.
		//
		// 방향은 판정과 같은 출처(스프라이트가 보는 쪽)를 쓴다. 다른 값을 쓰면
		// 이펙트가 날아가는 곳과 실제로 맞는 곳이 갈린다.
		const UTDCombatComponent* Combat = Owner->GetCombatComponent();
		FVector Facing = Combat ? Combat->GetFacingDirection().GetSafeNormal2D() : FVector::ZeroVector;
		if (Facing.IsNearlyZero())
		{
			Facing = Owner->GetActorForwardVector().GetSafeNormal2D();
		}

		// 위치는 몸 그대로 보낸다. VFXOffset 은 받는 쪽이 더한다 — 크기·위치 값은 표현이라
		// 각 화면이 적용해야 TD.SkillVFX 로 바꾼 값이 서버를 거치지 않고 바로 보인다.
		MulticastOnSkillFired(CastingSkillId, Owner->GetActorLocation(), Facing.Rotation());
	}

	// 효과마다 대상이 다를 수 있다 — 마법사 장판은 아군을 회복하면서 적을 때린다.
	// 같은 팀을 두 번 훑지 않도록 한 번 모은 것을 재사용한다.
	TMap<ETDSkillTarget, TArray<AActor*>> GatheredByTeam;

	for (const FTDSkillEffectRow& Effect : Progression->GetSkillEffects(CastingSkillId))
	{
		const TArray<AActor*>* Targets = GatheredByTeam.Find(Effect.TargetTeam);
		if (Targets == nullptr)
		{
			Targets = &GatheredByTeam.Add(Effect.TargetTeam, GatherTargets(*Row, Effect.TargetTeam));
		}

		const float Value = Effect.BaseValue + Effect.ValuePerLevel * (SkillLevel - 1);
		ApplyEffect(Effect, Value, *Row, *Targets);
	}
}

TArray<AActor*> UTDSkillComponent::GatherTargets(const FTDSkillRow& Row,
	ETDSkillTarget TargetTeam) const
{
	ATDCharacterBase* Owner = GetOwnerCharacter();

	if (Row.ShapeTag == TDTags::Skill_Shape_ForwardBox)
	{
		// 방향은 평타와 같은 출처를 쓴다 — 스프라이트가 보는 쪽이다.
		// 여기서 액터 회전을 쓰면 평타와 스킬이 서로 다른 곳을 때린다.
		const UTDCombatComponent* Combat = Owner ? Owner->GetCombatComponent() : nullptr;
		const FVector Facing = Combat ? Combat->GetFacingDirection() : FVector::ZeroVector;

		// 시트에는 전체 크기를 적고 여기서 절반으로 바꾼다. 상자 중심을 앞으로 Range/2 밀면
		// 몸에서 정확히 Range 만큼 뻗은 상자가 된다.
		const FVector HalfExtent(Row.Range * 0.5f, Row.Width * 0.5f, SkillBoxHalfHeight);
		return UTDCombatStatics::GatherTargetsInBox(
			Owner, Facing, HalfExtent, Row.Range * 0.5f, bDrawDebugShape, TargetTeam);
	}

	if (Row.ShapeTag == TDTags::Skill_Shape_SelfRadius)
	{
		return UTDCombatStatics::GatherTargetsInSphere(
			Owner, Row.Range, bDrawDebugShape, TargetTeam);
	}

	// Skill.Shape.Self — 범위를 훑지 않고 시전자에게만 간다.
	// 아군 대상이 생기면서 이 경우도 목록으로 표현할 수 있게 됐다 —
	// 예전처럼 빈 배열을 돌려주고 ApplyEffect 가 따로 분기할 필요가 없다.
	return Owner != nullptr && TargetTeam == ETDSkillTarget::Ally
		? TArray<AActor*>{ Owner } : TArray<AActor*>();
}

void UTDSkillComponent::ApplyEffect(const FTDSkillEffectRow& Effect, float Value,
	const FTDSkillRow& Row, const TArray<AActor*>& Targets)
{
	ATDCharacterBase* Owner = GetOwnerCharacter();
	if (Owner == nullptr)
	{
		return;
	}

	const FGameplayTag EffectTag = Effect.EffectTag;

	if (EffectTag == TDTags::Skill_Effect_Damage)
	{
		UTDCombatComponent* Combat = Owner->GetCombatComponent();

		for (AActor* Target : Targets)
		{
			// Value 는 피해량이 아니라 공격력 배율이다. ContextTags 는 조건부 장비 옵션
			// ("화염 스킬 +30%")을 평가하는 데 쓰인다.
			const FTDDamageResult Result =
				UTDCombatStatics::ApplyDamage(Owner, Target, Row.ContextTags, Value);

			if (Result.FinalDamage > 0.f && Combat != nullptr)
			{
				// 평타와 같은 델리게이트로 흘려보낸다. 데미지 텍스트와 히트 VFX 가
				// 스킬용 델리게이트를 따로 구독하지 않아도 되게 하려는 것이다.
				//
				// 접촉점은 대상 중심으로 둔다. 평타는 칼이 닿는 지점을 계산하지만
				// 범위 스킬에는 "닿은 지점" 이 하나가 아니다.
				Combat->NotifyHit(Target, Result.FinalDamage, Result.bCritical,
					Target->GetActorLocation());
			}
		}

		return;
	}

	// 회복은 대상 목록을 그대로 돈다. 자기만 회복하는 스킬은 시트에서
	// ShapeTag=Skill.Shape.Self + TargetTeam=Ally 로 적으면 목록에 자기만 들어온다.
	if (EffectTag == TDTags::Skill_Effect_Heal)
	{
		for (AActor* Target : Targets)
		{
			UTDCombatStatics::RestoreHealth(Target, Value);
		}
		return;
	}

	if (EffectTag == TDTags::Skill_Effect_RestoreMana)
	{
		for (AActor* Target : Targets)
		{
			UTDCombatStatics::RestoreMana(Target, Value);
		}
		return;
	}

	// ── 지속 효과 ──

	if (EffectTag == TDTags::Skill_Effect_Buff)
	{
		if (Effect.Duration <= 0.f || !Effect.StatTag.IsValid())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("스킬 '%s': 버프인데 Duration(%.2f) 이나 StatTag('%s') 가 비었다. "
					 "DT_SkillEffect 를 확인할 것."),
				*CastingSkillId.ToString(), Effect.Duration, *Effect.StatTag.ToString());
			return;
		}

		// 장비·패시브가 쓰는 것과 같은 스탯 소스다. 시간만 붙는다.
		TArray<FTDStatModifier> Modifiers;
		Modifiers.Emplace(Effect.StatTag, Effect.Op, Value);

		for (AActor* Target : Targets)
		{
			ATDCharacterBase* TargetChar = Cast<ATDCharacterBase>(Target);
			UTDStatComponent* Stats = TargetChar ? TargetChar->GetStatComponent() : nullptr;

			if (Stats != nullptr)
			{
				Stats->AddTimedSource(TDTags::Source_Skill, Modifiers, Effect.Duration);
			}
		}

		return;
	}

	if (EffectTag == TDTags::Skill_Effect_Invulnerable)
	{
		if (Effect.Duration <= 0.f)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("스킬 '%s': 무적인데 Duration 이 0 이다. 아무 일도 일어나지 않는다."),
				*CastingSkillId.ToString());
			return;
		}

		for (AActor* Target : Targets)
		{
			if (ATDCharacterBase* TargetChar = Cast<ATDCharacterBase>(Target))
			{
				TargetChar->SetInvulnerable(Effect.Duration);
			}
		}

		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("스킬 '%s': 모르는 효과 태그 '%s'. DT_SkillEffect 를 확인할 것."),
		*CastingSkillId.ToString(), *EffectTag.ToString());
}

// ── 종료 ──────────────────────────────────────────────────

void UTDSkillComponent::EndCast(bool bFired)
{
	if (!IsCasting())
	{
		return;
	}

	const FName SkillId = CastingSkillId;

	// 한 번도 발동하지 못했으면 쿨이 없다. 이동이나 CC 로 끊긴 캐스팅에 쿨을 걸면
	// 방해받은 쪽이 두 번 손해를 본다.
	// 검사만 건너뛰고 기록은 남기면 TD.DumpCooldown 에 쓸 수 없는 숫자가 뜬다.
	// 아예 걸지 않아야 화면과 실제가 일치한다.
	float Cooldown = 0.f;
	if (bFired && !IsSkillCooldownIgnored())
	{
		if (const FTDSkillRow* Row = GetCastingRow())
		{
			const UTDProgressionComponent* Progression = GetProgression();
			const int32 SkillLevel = Progression ? Progression->GetSkillLevel(SkillId) : 1;

			Cooldown = GetActualCooldown(*Row, SkillLevel);
		}
	}

	ClearCastTimers();
	bCostPaid = false;

	// 즉발 스킬은 발동 직후 여기로 온다. 날아가던 이펙트가 있으면 도착할 때까지 틱을 남긴다 —
	// 끄면 몸 앞에서 멈춘다. 다 도착하면 틱이 스스로 꺼진다.
	SetComponentTickEnabled(TravelingVFX.Num() > 0);

	MulticastOnCastEnded(SkillId, bFired, Cooldown);
}

void UTDSkillComponent::CancelCast()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	// 이미 발동했으면 쿨은 걸린다. 정신집중을 중간에 끊어도 들어간 피해는 되돌릴 수 없다.
	EndCast(bCostPaid);
}

void UTDSkillComponent::ServerCancelCast_Implementation()
{
	CancelCast();
}

void UTDSkillComponent::ClearCastTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(CastTimerHandle);
		Timers.ClearTimer(ChannelTickHandle);
		Timers.ClearTimer(ChannelEndHandle);
	}
}

// ── 이동 감시 ─────────────────────────────────────────────

void UTDSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 날아가는 이펙트는 모든 머신이 자기 화면에서 옮긴다.
	TickTravelingVFX();

	// 감시는 서버 몫이다. 클라이언트는 애초에 이동 입력을 막지만, 막힌 것을 뚫고
	// 움직이는 경우까지 서버가 걸러야 한다.
	if (GetOwnerRole() != ROLE_Authority || !IsCasting())
	{
		// 감시할 시전도 옮길 이펙트도 없으면 틱을 멈춘다.
		if (TravelingVFX.Num() == 0)
		{
			SetComponentTickEnabled(false);
		}
		return;
	}

	const FTDSkillRow* Row = GetCastingRow();
	const AActor* Owner = GetOwner();
	if (Row == nullptr || Owner == nullptr || Row->CastMovement == ETDSkillCastMovement::Free)
	{
		return;
	}

	// 시전 직후 잠깐은 봐준다.
	//
	// 클라이언트는 Multicast 가 도착해야 시전 중인 것을 알고 이동 입력을 막는다.
	// 그 사이(핑의 절반쯤)에는 이동 요청이 계속 오므로, 감시를 바로 켜면
	// 뛰면서 쓴 스킬이 서버에서 즉시 끊긴다.
	if (GetWorld()->GetTimeSeconds() - CastStartTime < MoveCancelGraceSeconds)
	{
		return;
	}

	if (Owner->GetVelocity().SizeSquared() > FMath::Square(MoveCancelSpeed))
	{
		UE_LOG(LogTemp, Log,
			TEXT("스킬 '%s': 이동으로 시전이 끊겼다."), *CastingSkillId.ToString());
		CancelCast();
	}
}

void UTDSkillComponent::ServerSetCastFacing_Implementation(FRotator Facing)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !IsCasting())
	{
		return;
	}

	// 제자리 회전만 허용된 스킬에서만 받는다. 다른 스킬에서 오면 위조이거나
	// 클라이언트가 상태를 잘못 알고 있는 것이다.
	if (GetCastMovement() != ETDSkillCastMovement::TurnOnly)
	{
		return;
	}

	// Yaw 만 쓴다. 2.5D 라 위아래로 겨냥할 일이 없고, Pitch 를 그대로 받으면
	// 스프라이트가 누워 버린다.
	const FRotator FlatFacing(0.f, Facing.Yaw, 0.f);
	Owner->SetActorRotation(FlatFacing);

	// 판정 방향까지 함께 돌린다.
	//
	// 히트박스는 액터 회전이 아니라 UTDCombatComponent 의 "마지막 이동 방향" 을 쓰는데,
	// 그 값은 속도에서 나온다. 제자리 회전은 속도가 0 이라 갱신되지 않으므로,
	// 여기서 직접 넣지 않으면 화면에서는 돌았는데 광선은 처음 방향으로 계속 나간다.
	if (UTDCombatComponent* Combat = GetOwnerCharacter() ? GetOwnerCharacter()->GetCombatComponent() : nullptr)
	{
		Combat->SetFacingDirection(FlatFacing.Vector());
	}
}

// ── 전 머신 동기화 ────────────────────────────────────────

void UTDSkillComponent::MulticastOnCastStarted_Implementation(
	FName SkillId, float CastTime, ETDSkillCastType CastType)
{
	CastingSkillId = SkillId;

	if (const UWorld* World = GetWorld())
	{
		CastStartTime = World->GetTimeSeconds();
	}

	// 제자리 스킬은 시전과 동시에 멈춘다.
	//
	// 뛰던 중에 스킬을 쓰면 관성이 남는데, 그대로 두면 서버의 이동 감시가 방금 시작한
	// 시전을 곧바로 끊는다. 입력을 막는 것만으로는 이미 붙은 속도가 사라지지 않는다.
	if (GetCastMovement() != ETDSkillCastMovement::Free)
	{
		ATDCharacterBase* Character = GetOwnerCharacter();

		// 시뮬레이션 프록시는 복제된 이동을 재생할 뿐이다. 여기서 멈추면 화면이 튄다.
		if (Character != nullptr && Character->GetLocalRole() >= ROLE_AutonomousProxy)
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
			}
		}
	}

	OnCastStarted.Broadcast(SkillId, CastTime, CastType);
}

void UTDSkillComponent::MulticastOnSkillFired_Implementation(
	FName SkillId, FVector Location, FRotator Facing)
{
	// 전용 서버는 화면이 없다. 이펙트를 만들어 봐야 아무도 보지 않는다.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ATDCharacterBase* Owner = GetOwnerCharacter();
	const UTDProgressionComponent* Progression = GetProgression();
	const FTDSkillRow* Row = Progression ? Progression->FindSkillRow(SkillId) : nullptr;

	if (Owner == nullptr || Row == nullptr)
	{
		return;
	}

	// 효과음은 이펙트와 따로 본다 — 이펙트가 없는 스킬도 소리는 날 수 있다.
	PlaySkillSFX(*Owner, *Row);

	if (Row->VFX.IsNull())
	{
		return;
	}

	// 소프트 참조라 처음 쓸 때 읽힌다. 스킬 이펙트는 몇 개뿐이고 작아서 동기로 읽는다 —
	// 첫 시전에 끊김이 보이면 그때 미리 읽어 두는 쪽으로 바꾼다.
	UNiagaraSystem* System = Row->VFX.LoadSynchronous();
	if (System == nullptr)
	{
		return;
	}

	float BaseSize = Row->VFXBaseSize;
	float Offset = Row->VFXOffset;
	float TravelTime = Row->VFXTravelTime;

	// TD.SkillVFX 로 맞추는 중이면 테이블 대신 그 값을 쓴다.
	if (const FDebugVFXTuning* Tuning = GetDebugVFXTuning().Find(SkillId))
	{
		BaseSize = Tuning->BaseSize;
		Offset = Tuning->Offset;
		TravelTime = Tuning->TravelTime;
	}

	// 판정 범위와 같은 크기로 키운다. 기준 크기를 모르면(0) 에셋 그대로 둔다.
	const float Scale = BaseSize > 0.f && Row->Range > 0.f
		? Row->Range / BaseSize : 1.f;

	UNiagaraComponent* Spawned = nullptr;

	if (Row->ShapeTag == TDTags::Skill_Shape_ForwardBox)
	{
		// 앞으로 뻗는 이펙트는 월드에 둔다. 시전자에 붙이면 날아가던 검기가 몸을 따라 휜다.
		// Location 은 시전 순간의 몸 위치다. 몸 앞 몇 cm 에서 시작할지는 여기서 더한다.
		const FVector Direction = Facing.Vector();
		const FVector Start = Location + Direction * Offset;

		Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, System, Start, Facing, FVector(Scale));

		// 이펙트 자체가 움직여야 꼬리가 그려지는 종류는 사거리 끝까지 옮긴다.
		const float Travel = Row->Range - Offset;
		if (Spawned != nullptr && TravelTime > 0.f && Travel > 0.f && GetWorld() != nullptr)
		{
			FTravelingVFX& Entry = TravelingVFX.AddDefaulted_GetRef();
			Entry.Component = Spawned;
			Entry.Start = Start;
			Entry.End = Start + Direction * Travel;
			Entry.StartTime = GetWorld()->GetTimeSeconds();
			Entry.Duration = TravelTime;

			// 틱은 원래 서버가 시전 중에만 켠다. 클라이언트와 시전이 끝난 뒤에도 옮기려면 켜야 한다.
			SetComponentTickEnabled(true);
		}
	}
	else
	{
		// 자기 주변 이펙트는 시전자에 붙인다. 이동하며 쓰는 정신집중(마력 폭풍)이
		// 월드에 고정되면 뛰어가는 동안 이펙트만 제자리에 남는다.
		//
		// 범위(SelfRadius) 이펙트는 발밑에 붙인다. 루트인 캡슐의 원점은 몸 한가운데라 그대로
		// 붙이면, 바닥을 원점으로 만든 장판·오라가 허리에서 시작해 머리 위로 솟는다 —
		// 크기를 키울수록 더 올라간다. 몸에 거는 효과(Self, 방벽)는 몸을 감싸야 하므로 그대로 둔다.
		FVector AttachOffset = FVector::ZeroVector;

		if (Row->ShapeTag == TDTags::Skill_Shape_SelfRadius)
		{
			if (const UCapsuleComponent* Capsule = Owner->GetCapsuleComponent())
			{
				// 붙은 뒤의 상대 위치라 부모 스케일이 곱해진다. 그래서 스케일 전 반높이를 쓴다.
				AttachOffset.Z = -Capsule->GetUnscaledCapsuleHalfHeight();
			}
		}

		Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(
			System, Owner->GetRootComponent(), NAME_None,
			AttachOffset, FRotator::ZeroRotator, FVector(Scale),
			EAttachLocation::SnapToTarget, /*bAutoDestroy=*/true, ENCPoolMethod::None);
	}

	if (Spawned == nullptr)
	{
		return;
	}

	// 언제 끌지. 한 번 터지고 끝나는 이펙트는 에셋이 알아서 사라지므로 손대지 않는다.
	// 반복하는 이펙트(장판)는 스킬이 지속되는 만큼만 보여야 한다.
	if (Row->CastType == ETDSkillCastType::Channel)
	{
		// 정신집중은 끝나는 시점을 모른다 — 끊길 수 있다. 시전 종료 방송이 끈다.
		StopChannelVFX();
		ActiveChannelVFX = Spawned;
		return;
	}

	// 지속 효과가 있으면 그 시간만큼 보인다 — 방벽의 무적 1초, 전열 강화의 버프 5초.
	float Lifetime = 0.f;
	for (const FTDSkillEffectRow& Effect : Progression->GetSkillEffects(SkillId))
	{
		Lifetime = FMath::Max(Lifetime, Effect.Duration);
	}

	if (Lifetime > 0.f && GetWorld() != nullptr)
	{
		TWeakObjectPtr<UNiagaraComponent> WeakSpawned = Spawned;
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle,
			FTimerDelegate::CreateWeakLambda(this, [WeakSpawned]()
			{
				if (WeakSpawned.IsValid())
				{
					WeakSpawned->Deactivate();
				}
			}),
			Lifetime, /*bLoop=*/false);
	}
}

void UTDSkillComponent::TickTravelingVFX()
{
	if (TravelingVFX.Num() == 0 || GetWorld() == nullptr)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	for (int32 Index = TravelingVFX.Num() - 1; Index >= 0; --Index)
	{
		FTravelingVFX& Entry = TravelingVFX[Index];

		// 이펙트가 스스로 먼저 사라졌다. 옮길 것이 없다.
		if (!Entry.Component.IsValid())
		{
			TravelingVFX.RemoveAtSwap(Index);
			continue;
		}

		const float Alpha = FMath::Clamp((Now - Entry.StartTime) / Entry.Duration, 0.f, 1.f);
		Entry.Component->SetWorldLocation(FMath::Lerp(Entry.Start, Entry.End, Alpha));

		if (Alpha >= 1.f)
		{
			// 도착했다. 새 입자만 멈추고 남은 꼬리는 스스로 사라지게 둔다.
			Entry.Component->Deactivate();
			TravelingVFX.RemoveAtSwap(Index);
		}
	}
}

TMap<FName, UTDSkillComponent::FDebugVFXTuning>& UTDSkillComponent::GetDebugVFXTuning()
{
	// PIE 를 다시 켜도 남는다. 여러 번 시전해 보며 맞추는 도구라 그편이 낫다 —
	// 에디터를 끄거나 TD.SkillVFX clear 로 지운다.
	static TMap<FName, FDebugVFXTuning> Tuning;
	return Tuning;
}

void UTDSkillComponent::StopChannelVFX()
{
	if (ActiveChannelVFX.IsValid())
	{
		// Deactivate 는 새 입자만 멈추고 남은 입자는 마저 사라지게 둔다. 뚝 끊기지 않는다.
		ActiveChannelVFX->Deactivate();
	}

	ActiveChannelVFX.Reset();
}

void UTDSkillComponent::PlaySkillSFX(ATDCharacterBase& Owner, const FTDSkillRow& Row)
{
	if (Row.SFX.IsNull())
	{
		return;
	}

	// 이펙트와 같은 이유로 처음 쓸 때 동기로 읽는다. 짧은 효과음 몇 개뿐이다.
	USoundBase* Sound = Row.SFX.LoadSynchronous();
	if (Sound == nullptr)
	{
		return;
	}

	// 시전자에 붙인다. 뛰면서 쓰는 정신집중(마력 폭풍)의 소리가 제자리에 남지 않게 —
	// 캐릭터가 사라지면(접속 종료) 같이 멈춘다. 볼륨 설정은 사운드 에셋의 Submix(SM_SFX)가 받는다.
	UAudioComponent* Audio = UGameplayStatics::SpawnSoundAttached(
		Sound, Owner.GetRootComponent(), NAME_None, FVector::ZeroVector,
		EAttachLocation::SnapToTarget, /*bStopWhenAttachedToDestroyed=*/true);

	// 정신집중은 끝나는 시점을 모른다. 시전 종료 방송이 이펙트와 함께 끈다.
	if (Audio != nullptr && Row.CastType == ETDSkillCastType::Channel)
	{
		StopChannelSFX();
		ActiveChannelSFX = Audio;
	}
}

void UTDSkillComponent::StopChannelSFX()
{
	if (ActiveChannelSFX.IsValid())
	{
		// 뚝 끊기지 않게 짧게 줄여서 끈다.
		ActiveChannelSFX->FadeOut(0.2f, 0.f);
	}

	ActiveChannelSFX.Reset();
}

void UTDSkillComponent::MulticastOnCastEnded_Implementation(
	FName SkillId, bool bFired, float Cooldown)
{
	CastingSkillId = NAME_None;

	// 정신집중이 끝나거나 끊겼다. 장판을 걷고 소리를 끈다.
	StopChannelVFX();
	StopChannelSFX();

	if (bFired && Cooldown > 0.f)
	{
		if (const UWorld* World = GetWorld())
		{
			CooldownEndTimes.Add(SkillId, World->GetTimeSeconds() + Cooldown);
		}
	}

	OnCastEnded.Broadcast(SkillId, bFired);
}
