#include "UI/ViewModel/TDBossViewModel.h"

#include "Abilities/TDAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Character/TDBossCharacter.h"

void UTDBossViewModel::SetSource(ATDBossCharacter* InBoss)
{
	if (!IsValid(InBoss) || InBoss->IsActorBeingDestroyed()) InBoss = nullptr;
	if (InBoss && Source.Get() == InBoss && BoundASC.Get() == InBoss->GetAbilitySystemComponent()){
		RefreshAll();
		return;
	}
	UnbindSource();
	Source = InBoss;
	if (InBoss){
		InBoss->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleSourceEndPlay);
		InBoss->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleSourceDestroyed);
		InBoss->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleDeathStateChanged);
		InBoss->OnRespawn.AddUniqueDynamic(this, &ThisClass::HandleDeathStateChanged);
		BoundASC = InBoss->GetAbilitySystemComponent();
		if (UAbilitySystemComponent* ASC = BoundASC.Get()){
			HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(
					                  UTDAttributeSet::GetHealthAttribute())
			                  .AddUObject(this, &ThisClass::HandleHealthChanged);
			MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(
					                     UTDAttributeSet::GetMaxHealthAttribute())
			                     .AddUObject(this, &ThisClass::HandleHealthChanged);
		}
	}
	// 구독 전에 감소한 HP와 사망 상태도 즉시 읽는다.
	RefreshAll();
}

ATDBossCharacter* UTDBossViewModel::GetSource() const
{
	return Source.Get();
}

void UTDBossViewModel::RefreshAll()
{
	const ATDBossCharacter* Boss = Source.Get();
	const UAbilitySystemComponent* ASC = Boss ? BoundASC.Get() : nullptr;
	const FText NewName = Boss ? Boss->GetDisplayName() : FText::GetEmpty();
	// 클라이언트에서는 수신된 값만 읽는다. Level 복제와 변경 알림은 게임플레이 측 책임이다.
	const int32 NewLevel = Boss ? FMath::Max(0, Boss->GetLevel()) : 0;
	float NewMaximum = ASC
		                   ? ASC->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute())
		                   : 0.f;
	NewMaximum = FMath::IsFinite(NewMaximum) ? FMath::Max(0.f, NewMaximum) : 0.f;
	float NewHealth = ASC ? ASC->GetNumericAttribute(UTDAttributeSet::GetHealthAttribute()) : 0.f;
	NewHealth = FMath::IsFinite(NewHealth) ? FMath::Clamp(NewHealth, 0.f, NewMaximum) : 0.f;
	if (!BossName.EqualTo(NewName))
		UE_MVVM_SET_PROPERTY_VALUE(BossName, NewName);
	UE_MVVM_SET_PROPERTY_VALUE(BossLevel, NewLevel);
	UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewMaximum);
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent,
	                           NewMaximum > KINDA_SMALL_NUMBER ? NewHealth / NewMaximum : 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(bHasBoss, Boss != nullptr);
	// HP 0만으로 사망을 추정하지 않는다. 초기 복제 중에도 0일 수 있다.
	UE_MVVM_SET_PROPERTY_VALUE(bIsDead, Boss && Boss->IsDead());
	OnDisplayChanged.Broadcast();
}

void UTDBossViewModel::UnbindSource()
{
	if (ATDBossCharacter* Boss = Source.Get()){
		Boss->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleSourceEndPlay);
		Boss->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleSourceDestroyed);
		Boss->OnDeath.RemoveDynamic(this, &ThisClass::HandleDeathStateChanged);
		Boss->OnRespawn.RemoveDynamic(this, &ThisClass::HandleDeathStateChanged);
	}
	if (UAbilitySystemComponent* ASC = BoundASC.Get()){
		ASC->GetGameplayAttributeValueChangeDelegate(UTDAttributeSet::GetHealthAttribute()).Remove(
				HealthHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UTDAttributeSet::GetMaxHealthAttribute()).
		     Remove(MaxHealthHandle);
	}
	HealthHandle.Reset();
	MaxHealthHandle.Reset();
	BoundASC.Reset();
	Source.Reset();
}

void UTDBossViewModel::HandleHealthChanged(const FOnAttributeChangeData& Data) { RefreshAll(); }
void UTDBossViewModel::HandleDeathStateChanged() { RefreshAll(); }

void UTDBossViewModel::HandleSourceEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	SetSource(nullptr);
}

void UTDBossViewModel::HandleSourceDestroyed(AActor* Actor) { SetSource(nullptr); }

void UTDBossViewModel::BeginDestroy()
{
	UnbindSource();
	Super::BeginDestroy();
}
