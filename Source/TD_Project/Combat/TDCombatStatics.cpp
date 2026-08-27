#include "Combat/TDCombatStatics.h"

#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Character/TDCharacterBase.h"
#include "Core/TDGameplayTags.h"
#include "GameplayEffect.h"
#include "Stats/TDStatComponent.h"

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

	return Result;
}

void UTDCombatStatics::ApplyRawDamage(AActor* Target, float Amount)
{
	ATDCharacterBase* TargetChar = Cast<ATDCharacterBase>(Target);
	if (TargetChar == nullptr || !TargetChar->HasAuthority() || Amount <= 0.f)
	{
		return;
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