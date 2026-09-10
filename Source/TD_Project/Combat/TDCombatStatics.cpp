#include "Combat/TDCombatStatics.h"

#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Character/TDCharacterBase.h"
#include "Character/TDEnemyBase.h" 
#include "Core/TDGameplayTags.h"
#include "Data/TDZoneEnvironmentRow.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Game/TDGameMode.h"
#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"
#include "Player/TDPlayerState.h"
#include "Stats/TDStatComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TDInteractionFlowComponent.h"

FTDDamageResult UTDCombatStatics::ApplyDamage(AActor* Attacker, AActor* Target,
	const FGameplayTagContainer& ContextTags, float DamageMultiplier)
{
	const FTDDamageResult NoDamage;

	// 배율 0 은 "피해 효과가 없다" 는 뜻이다. 그대로 흘려보내면 계산 마지막의
	// 바닥값(최소 1) 때문에 때린 것이 되어 버린다.
	if (DamageMultiplier <= 0.f)
	{
		return NoDamage;
	}

	ATDCharacterBase* AttackerChar = Cast<ATDCharacterBase>(Attacker);
	ATDCharacterBase* TargetChar = Cast<ATDCharacterBase>(Target);

	if (AttackerChar == nullptr || TargetChar == nullptr)
	{
		return NoDamage;
	}

	// 데미지는 서버만 계산한다. 클라이언트 호출은 조용히 무시.
	if (!TargetChar->HasAuthority())
	{
		return NoDamage;
	}

	// 시체는 때릴 수 없고, 시체가 때릴 수도 없다.
	if (AttackerChar->IsDead() || TargetChar->IsDead())
	{
		return NoDamage;
	}

	// 플레이어가 대화창이나 컷씬 등으로 게임플레이가 잠겨있는 동안에는, 몬스터나 다른 캐릭터의 공격이 들어와도 데미지를 받지 않는다
	if (APlayerController* TargetController =
	Cast<APlayerController>(
		TargetChar->GetController()))
	{
		const UTDInteractionFlowComponent* Flow =
			TargetController->FindComponentByClass<
				UTDInteractionFlowComponent>();

		if (Flow != nullptr
			&& Flow->IsGameplayLocked())
		{
			return NoDamage;
		}
	}
	
	// 안전지대에서는 전투가 일어나지 않는다(DT_ZoneEnvironment.bIsSafeZone).
	//
	// **맞는 쪽 기준**이다. 때리는 쪽을 보면 안전지대 밖에서 마을 안으로 원거리
	// 공격을 넣는 것을 막지 못한다.
	//
	// ApplyRawDamage 가 아니라 여기에 두는 이유는 그쪽이 치트(TD.Damage)와 환경 피해도
	// 함께 쓰는 저수준 통로이기 때문이다. 마을에서 회복을 테스트하려면 치트는 통해야 한다.
	if (IsInSafeZone(TargetChar))
	{
		return NoDamage;
	}

	UTDStatComponent* AttackerStats = AttackerChar->GetStatComponent();
	if (AttackerStats == nullptr)
	{
		return NoDamage;
	}

	// ── 스탯 스냅샷: 여기가 스탯 시스템과 전투의 공식 접점 ──
	FTDDamageInput Input;

	// 물리·마법 중 높은 쪽을 주력으로. GetCombatPower 와 같은 판단이다.
	//
	// 스킬이 "나는 물리 스킬이다" 를 명시하는 방식도 검토했으나 넣지 않기로 했다.
	// 직업 = 캐릭터라서 전사는 물리가, 마법사는 마법이 늘 높고, 그러면 이 max 가
	// 알아서 맞는 쪽을 고른다. DT_Skill 에 열을 하나 더 두면 기획이 18 줄을 채워야
	// 하는데 그 값이 직업만 보면 뻔하다.
	//
	// 한 직업이 물리·마법 스킬을 섞어 갖게 되면 그때 DT_Skill 에 DamageStatTag 를
	// 더하고 여기로 넘긴다.
	const float Physical = AttackerStats->GetStatWithContext(TDTags::Stat_Offense_Damage_Physical, ContextTags);
	const float Magical = AttackerStats->GetStatWithContext(TDTags::Stat_Offense_Damage_Magical, ContextTags);
	Input.AttackDamage = FMath::Max(Physical, Magical) * DamageMultiplier;

	Input.CritChance = AttackerStats->GetStatWithContext(TDTags::Stat_Offense_CritChance, ContextTags);
	Input.CritDamage = AttackerStats->GetStatWithContext(TDTags::Stat_Offense_CritDamage, ContextTags);
	Input.ArmorPenetration = AttackerStats->GetStatWithContext(TDTags::Stat_Offense_ArmorPenetration, ContextTags);

	Input.TargetArmor = TargetChar->GetStat(TDTags::Stat_Defense_Armor);
	Input.TargetDamageReduction = TargetChar->GetStat(TDTags::Stat_Defense_DamageReduction);

	// 보스 추가 피해는 대상이 보스일 때만 붙는다. "보스인가" 는 몬스터 테이블을 읽어야
	// 알 수 있어서 계산 함수 안에서 판단하지 않는다 — 그러면 순수 함수가 아니게 된다.
	const ATDEnemyBase* TargetEnemy = Cast<ATDEnemyBase>(TargetChar);
	if (TargetEnemy != nullptr && TargetEnemy->IsBoss())
	{
		Input.BossDamageBonus =
			AttackerStats->GetStatWithContext(TDTags::Stat_Offense_BossDamage, ContextTags);
	}

	// 난수는 여기서 굴린다 — 순수 함수에 주입하기로 한 그 지점이다.
	const FTDDamageResult Result = TDCombat::CalculateDamage(Input, FMath::FRand());

	ApplyRawDamage(Target, Result.FinalDamage);

	// 이 타격으로 죽었다면 보상은 때린 쪽 몫이다. 죽는 순간의 공격자가 곧 킬러이므로
	// "마지막 타격자"를 따로 저장·복제할 필요가 없다.
	if (TargetChar->IsDead())
	{
		if (ATDEnemyBase* DeadEnemy = Cast<ATDEnemyBase>(TargetChar))
		{
			DeadEnemy->GrantRewards(AttackerChar);
		}
	}
	else if (Result.FinalDamage > 0.f)
	{
		// 살아남았으면 "맞았다" — 경직·어그로·피격 연출은 대상 쪽 일이다.
		TargetChar->ReceiveHit(AttackerChar, Result.FinalDamage, Result.bCritical);
	}
	
	return Result;
}

void UTDCombatStatics::ApplyRawDamage(AActor* Target, float Amount)
{
	ATDCharacterBase* TargetChar = Cast<ATDCharacterBase>(Target);
	if (TargetChar == nullptr || !TargetChar->HasAuthority() || Amount <= 0.f)
	{
		return;
	}
	// 플레이어가 대화/컷씬 등으로 게임플레이가 잠긴 동안엔 노 데미지
	if (APlayerController* TargetController =
	Cast<APlayerController>(
		TargetChar->GetController()))
	{
		const UTDInteractionFlowComponent* Flow =
			TargetController->FindComponentByClass<
				UTDInteractionFlowComponent>();

		if (Flow != nullptr
			&& Flow->IsGameplayLocked())
		{
			return;
		}
	}
	
	UAbilitySystemComponent* TargetASC = TargetChar->GetAbilitySystemComponent();
	if (TargetASC == nullptr)
	{
		return;
	}

	// 즉석에서 만드는 일회용 GE. 에셋이 아니라 메모리에만 존재한다(BP 참조 금지 컨벤션).
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("GE_TDDamage"));
	Effect->DurationPolicy = EGameplayEffectDurationType::Instant;

	// "IncomingDamage 에 Amount 를 더한다"는 모디파이어 하나짜리 GE.
	FGameplayModifierInfo Mod;
	Mod.Attribute = UTDAttributeSet::GetIncomingDamageAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;
	Mod.ModifierMagnitude = FScalableFloat(Amount);
	Effect->Modifiers.Add(Mod);

	TargetASC->ApplyGameplayEffectToSelf(Effect, 1.f, TargetASC->MakeEffectContext());
	// → 이 호출이 2단계에서 만든 PostGameplayEffectExecute 를 발동시킨다.
}

namespace
{
	/**
	 * 회복 공통부. 대상을 검증하고 현재값을 최대치까지 올린다.
	 *
	 * @return 값이 실제로 바뀌었으면 true.
	 */
	bool RestoreAttribute(AActor* Target, float Amount,
		const FGameplayAttribute& CurrentAttribute, const FGameplayAttribute& MaxAttribute)
	{
		ATDCharacterBase* TargetChar = Cast<ATDCharacterBase>(Target);
		if (TargetChar == nullptr || !TargetChar->HasAuthority() || Amount <= 0.f)
		{
			return false;
		}

		// 시체는 회복하지 않는다. 부활은 포션이 아니라 별도 경로여야 한다.
		if (TargetChar->IsDead())
		{
			return false;
		}

		UAbilitySystemComponent* ASC = TargetChar->GetAbilitySystemComponent();
		if (ASC == nullptr)
		{
			return false;
		}

		const float Current = ASC->GetNumericAttribute(CurrentAttribute);
		const float Max = ASC->GetNumericAttribute(MaxAttribute);

		// 이미 가득 찼으면 실패로 돌려준다. 부르는 쪽이 아이템을 소모할지 판단한다 —
		// 풀피에서 포션이 그냥 사라지면 안 되기 때문이다.
		if (Current >= Max)
		{
			return false;
		}

		ASC->SetNumericAttributeBase(CurrentAttribute, FMath::Min(Current + Amount, Max));
		return true;
	}
}

bool UTDCombatStatics::RestoreHealth(AActor* Target, float Amount)
{
	return RestoreAttribute(Target, Amount,
		UTDAttributeSet::GetHealthAttribute(), UTDAttributeSet::GetMaxHealthAttribute());
}

bool UTDCombatStatics::RestoreMana(AActor* Target, float Amount)
{
	return RestoreAttribute(Target, Amount,
		UTDAttributeSet::GetManaAttribute(), UTDAttributeSet::GetMaxManaAttribute());
}

bool UTDCombatStatics::ConsumeMana(AActor* Target, float Amount)
{
	ATDCharacterBase* TargetChar = Cast<ATDCharacterBase>(Target);
	if (TargetChar == nullptr || !TargetChar->HasAuthority())
	{
		return false;
	}

	// 공짜 스킬. 소모할 것이 없으니 성공이다.
	if (Amount <= 0.f)
	{
		return true;
	}

	UAbilitySystemComponent* ASC = TargetChar->GetAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return false;
	}

	const float Current = ASC->GetNumericAttribute(UTDAttributeSet::GetManaAttribute());
	if (Current < Amount)
	{
		return false;
	}

	ASC->SetNumericAttributeBase(UTDAttributeSet::GetManaAttribute(), Current - Amount);
	return true;
}

float UTDCombatStatics::GetMana(const AActor* Actor)
{
	const ATDCharacterBase* Character = Cast<ATDCharacterBase>(Actor);
	const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;

	return ASC ? ASC->GetNumericAttribute(UTDAttributeSet::GetManaAttribute()) : 0.f;
}

namespace
{
	/**
	 * 겹친 것들 중 실제로 때릴 수 있는 대상만 골라낸다.
	 *
	 * 모양(상자·구)이 달라도 이 규칙은 같아야 하므로 한 곳에 둔다.
	 * 같은 액터가 콜리전 여러 개로 두 번 잡히는 것도 여기서 거른다.
	 */
	TArray<AActor*> FilterHostileTargets(const ATDCharacterBase* Attacker,
		const TArray<FOverlapResult>& Overlaps)
	{
		TArray<AActor*> Targets;
		TSet<AActor*> Seen;

		for (const FOverlapResult& Overlap : Overlaps)
		{
			ATDCharacterBase* Candidate = Cast<ATDCharacterBase>(Overlap.GetActor());
			if (Candidate == nullptr || Candidate->IsDead() || Seen.Contains(Candidate))
			{
				continue;
			}

			// 같은 팀은 때리지 않는다(§10-④ 임시 규칙).
			if (Candidate->GetGenericTeamId() == Attacker->GetGenericTeamId())
			{
				continue;
			}

			Seen.Add(Candidate);
			Targets.Add(Candidate);
		}

		return Targets;
	}
}

TArray<AActor*> UTDCombatStatics::GatherTargetsInBox(const AActor* Attacker, FVector Direction,
	FVector HalfExtent, float ForwardOffset, bool bDrawDebug)
{
	const ATDCharacterBase* AttackerChar = Cast<ATDCharacterBase>(Attacker);
	if (AttackerChar == nullptr)
	{
		return TArray<AActor*>();
	}

	UWorld* World = AttackerChar->GetWorld();
	if (World == nullptr)
	{
		return TArray<AActor*>();
	}

	// 방향을 인자로 받는 이유는 액터 회전과 눈에 보이는 방향이 다르기 때문이다.
	// 스프라이트는 마지막 이동 방향(ABP SetDirectionality ← Velocity)을 따라가므로,
	// 액터 회전으로 판정하면 화면에서 보는 쪽과 맞는 곳이 어긋난다.
	//
	// 0 벡터가 오면 액터 정면으로 떨어뜨린다 — 그러지 않으면 MakeFromX 가
	// NaN 회전을 만들어 판정이 통째로 사라진다.
	FVector Facing = Direction.GetSafeNormal2D();
	if (Facing.IsNearlyZero())
	{
		Facing = AttackerChar->GetActorForwardVector().GetSafeNormal2D();
	}

	const FQuat BoxRotation = FRotationMatrix::MakeFromX(Facing).ToQuat();
	const FVector Center = AttackerChar->GetActorLocation() + Facing * ForwardOffset;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(AttackerChar);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, BoxRotation,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(HalfExtent), Params);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		// 질의와 같은 회전으로 그린다. 다른 값을 쓰면 눈에 보이는 상자와 실제로
		// 맞는 범위가 달라져, 사거리를 맞추려다 엉뚱한 곳을 고치게 된다.
		DrawDebugBox(World, Center, HalfExtent, BoxRotation, FColor::Red, false, 0.5f);
	}
#endif

	return FilterHostileTargets(AttackerChar, Overlaps);
}

TArray<AActor*> UTDCombatStatics::GatherTargetsInSphere(const AActor* Attacker,
	float Radius, bool bDrawDebug)
{
	const ATDCharacterBase* AttackerChar = Cast<ATDCharacterBase>(Attacker);
	if (AttackerChar == nullptr || Radius <= 0.f)
	{
		return TArray<AActor*>();
	}

	UWorld* World = AttackerChar->GetWorld();
	if (World == nullptr)
	{
		return TArray<AActor*>();
	}

	const FVector Center = AttackerChar->GetActorLocation();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(AttackerChar);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(World, Center, Radius, 16, FColor::Red, false, 0.5f);
	}
#endif

	return FilterHostileTargets(AttackerChar, Overlaps);
}

bool UTDCombatStatics::IsInSafeZone(const AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	const ATDPlayerState* PlayerState = Pawn ? Pawn->GetPlayerState<ATDPlayerState>() : nullptr;

	if (PlayerState == nullptr)
	{
		// 몬스터에는 PlayerState 가 없다. 존을 알 방법이 없으므로 안전지대가 아닌 것으로 본다.
		return false;
	}

	const UWorld* World = Actor->GetWorld();
	const ATDGameMode* GameMode = World ? World->GetAuthGameMode<ATDGameMode>() : nullptr;

	if (GameMode == nullptr)
	{
		// 클라이언트다. 테이블 조회는 GameMode 를 거치므로 여기서는 판단할 수 없다.
		return false;
	}

	const FTDZoneEnvironmentRow* Row = GameMode->FindZoneRow(PlayerState->GetCurrentZoneId());
	return Row != nullptr && Row->bIsSafeZone;
}
