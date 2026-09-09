#include "Skill/TDSkillComponent.h"

#include "Character/TDCharacterBase.h"
#include "Combat/TDCombatComponent.h"
#include "Combat/TDCombatStatics.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDSkillEffectRow.h"
#include "Data/TDSkillRow.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TDInteractionFlowComponent.h"
#include "Stats/TDProgressionComponent.h"
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

float UTDSkillComponent::GetActualCooldown(const FTDSkillRow& Row) const
{
	const ATDCharacterBase* Owner = GetOwnerCharacter();
	if (Owner == nullptr)
	{
		return Row.Cooldown;
	}

	// 실제 쿨타임 = 기본 ÷ (1 + 회복률). 평타(UTDCombatComponent::CanAttack)와 같은 규칙이다.
	const float Recovery = FMath::Max(0.f, Owner->GetStat(TDTags::Stat_Utility_CooldownRecoveryRate));
	return Row.Cooldown / (1.f + Recovery);
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
	}

	const TArray<AActor*> Targets = GatherTargets(*Row);

	for (const FTDSkillEffectRow& Effect : Progression->GetSkillEffects(CastingSkillId))
	{
		const float Value = Effect.BaseValue + Effect.ValuePerLevel * (SkillLevel - 1);
		ApplyEffect(Effect.EffectTag, Value, *Row, Targets);
	}
}

TArray<AActor*> UTDSkillComponent::GatherTargets(const FTDSkillRow& Row) const
{
	AActor* Owner = GetOwner();

	if (Row.ShapeTag == TDTags::Skill_Shape_ForwardBox)
	{
		// 시트에는 전체 크기를 적고 여기서 절반으로 바꾼다. 상자 중심을 앞으로 Range/2 밀면
		// 몸에서 정확히 Range 만큼 뻗은 상자가 된다.
		const FVector HalfExtent(Row.Range * 0.5f, Row.Width * 0.5f, SkillBoxHalfHeight);
		return UTDCombatStatics::GatherTargetsInBox(
			Owner, HalfExtent, Row.Range * 0.5f, bDrawDebugShape);
	}

	if (Row.ShapeTag == TDTags::Skill_Shape_SelfRadius)
	{
		return UTDCombatStatics::GatherTargetsInSphere(Owner, Row.Range, bDrawDebugShape);
	}

	// Skill.Shape.Self — 대상을 찾지 않는다. 회복·버프는 아래에서 시전자에게 간다.
	return TArray<AActor*>();
}

void UTDSkillComponent::ApplyEffect(FGameplayTag EffectTag, float Value,
	const FTDSkillRow& Row, const TArray<AActor*>& Targets)
{
	ATDCharacterBase* Owner = GetOwnerCharacter();
	if (Owner == nullptr)
	{
		return;
	}

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

	// 회복은 언제나 시전자에게 간다. 파티원 회복은 아군을 모으는 다른 수집이 필요하고
	// (지금 GatherTargets 는 적만 모은다) 그런 스킬이 아직 없다.
	if (EffectTag == TDTags::Skill_Effect_Heal)
	{
		UTDCombatStatics::RestoreHealth(Owner, Value);
		return;
	}

	if (EffectTag == TDTags::Skill_Effect_RestoreMana)
	{
		UTDCombatStatics::RestoreMana(Owner, Value);
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
			Cooldown = GetActualCooldown(*Row);
		}
	}

	ClearCastTimers();
	bCostPaid = false;
	SetComponentTickEnabled(false);

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

	// 감시는 서버 몫이다. 클라이언트는 애초에 이동 입력을 막지만, 막힌 것을 뚫고
	// 움직이는 경우까지 서버가 걸러야 한다.
	if (GetOwnerRole() != ROLE_Authority || !IsCasting())
	{
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
	Owner->SetActorRotation(FRotator(0.f, Facing.Yaw, 0.f));
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

void UTDSkillComponent::MulticastOnCastEnded_Implementation(
	FName SkillId, bool bFired, float Cooldown)
{
	CastingSkillId = NAME_None;

	if (bFired && Cooldown > 0.f)
	{
		if (const UWorld* World = GetWorld())
		{
			CooldownEndTimes.Add(SkillId, World->GetTimeSeconds() + Cooldown);
		}
	}

	OnCastEnded.Broadcast(SkillId, bFired);
}
