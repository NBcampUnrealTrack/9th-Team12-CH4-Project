#include "Abilities/TDAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Character/TDCharacterBase.h"

UTDAttributeSet::UTDAttributeSet()
{
	// 스탯 컴포넌트가 값을 넣어주기 전까지 쓰이는 임시값이다.
	// 0 으로 두면 최대 체력이 정해지기 전에 클램프가 걸려 현재 체력이 0 이 된다.
	InitMaxHealth(1.f);
	InitHealth(1.f);
	InitMaxMana(1.f);
	InitMana(1.f);
}

void UTDAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 어트리뷰트는 ReplicationMode 와 무관하게 전원에게 간다.
	// 남의 체력바를 그려야 하기 때문이다. Mixed/Minimal 이 가르는 것은 GameplayEffect 쪽이다.
	//
	// COND_None + REPNOTIFY_Always 는 GAS 의 표준 설정이다.
	// 값이 같아도 알림을 받아야 예측이 빗나갔을 때 되돌릴 수 있다.
	DOREPLIFETIME_CONDITION_NOTIFY(UTDAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTDAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTDAttributeSet, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UTDAttributeSet, MaxMana, COND_None, REPNOTIFY_Always);
}

void UTDAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
		return;
	}

	if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxMana());
		return;
	}

	// 최대치가 0 이하로 내려가면 현재값 클램프가 무너진다.
	if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxManaAttribute())
	{
		NewValue = FMath::Max(1.f, NewValue);
	}
}

void UTDAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTDAttributeSet, Health, OldValue);
}

void UTDAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTDAttributeSet, MaxHealth, OldValue);
}

void UTDAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTDAttributeSet, Mana, OldValue);
}

void UTDAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UTDAttributeSet, MaxMana, OldValue);
}

void UTDAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 이 훅은 모든 어트리뷰트 변경마다 불린다. 데미지 우편함일 때만 처리한다.
	if (Data.EvaluatedData.Attribute != GetIncomingDamageAttribute())
	{
		return;
	}

	// 우편함을 비우고 값을 꺼낸다. 남겨두면 다음 피격에 합산되는 사고가 난다.
	const float Damage = GetIncomingDamage();
	SetIncomingDamage(0.f);

	if (Damage <= 0.f)
	{
		return;
	}

	SetHealth(FMath::Clamp(GetHealth() - Damage, 0.f, GetMaxHealth()));

	if (GetHealth() > 0.f)
	{
		return;
	}

	// 체력 0 — 월드의 캐릭터(Avatar)에게 사망을 알린다.
	// Avatar 인 이유: 플레이어의 ASC 는 PlayerState 에 있지만 죽는 것은 월드의 캐릭터다.
	// HandleDeath 의 bIsDead 가드가 중복 호출을 막아준다.
	AActor* Avatar = Data.Target.AbilityActorInfo.IsValid()
		? Data.Target.AbilityActorInfo->AvatarActor.Get()
		: nullptr;

	if (ATDCharacterBase* Character = Cast<ATDCharacterBase>(Avatar))
	{
		Character->HandleDeath();
	}
}