#include "Combat/TDCombatStatics.h"

#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Character/TDCharacterBase.h"
#include "Character/TDEnemyBase.h" 
#include "Core/TDGameplayTags.h"
#include "GameplayEffect.h"
#include "Stats/TDStatComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TDInteractionFlowComponent.h"

FTDDamageResult UTDCombatStatics::ApplyDamage(AActor* Attacker, AActor* Target,
	const FGameplayTagContainer& ContextTags)
{
	const FTDDamageResult NoDamage;

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
	
	UTDStatComponent* AttackerStats = AttackerChar->GetStatComponent();
	if (AttackerStats == nullptr)
	{
		return NoDamage;
	}

	// ── 스탯 스냅샷: 여기가 스탯 시스템과 전투의 공식 접점 ──
	FTDDamageInput Input;

	// 물리·마법 중 높은 쪽을 주력으로. GetCombatPower 와 같은 판단이다.
	// TODO: 스킬 도입 시 스킬이 명시한 태그로 교체한다.
	const float Physical = AttackerStats->GetStatWithContext(TDTags::Stat_Offense_Damage_Physical, ContextTags);
	const float Magical = AttackerStats->GetStatWithContext(TDTags::Stat_Offense_Damage_Magical, ContextTags);
	Input.AttackDamage = FMath::Max(Physical, Magical);

	Input.CritChance = AttackerStats->GetStatWithContext(TDTags::Stat_Offense_CritChance, ContextTags);
	Input.CritDamage = AttackerStats->GetStatWithContext(TDTags::Stat_Offense_CritDamage, ContextTags);
	Input.ArmorPenetration = AttackerStats->GetStatWithContext(TDTags::Stat_Offense_ArmorPenetration, ContextTags);

	Input.TargetArmor = TargetChar->GetStat(TDTags::Stat_Defense_Armor);
	Input.TargetDamageReduction = TargetChar->GetStat(TDTags::Stat_Defense_DamageReduction);

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