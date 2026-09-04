#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "CoreMinimal.h"
#include "TDAttributeSet.generated.h"

/**
 * 하나의 어트리뷰트에 대해 Getter/Setter/Initter 를 한꺼번에 만든다.
 * GAS 프로젝트에서 관례적으로 쓰는 매크로다.
 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * GAS 가 다루고 클라이언트가 알아야 하는 수치.
 *
 * 여기 있는 것과 UTDStatComponent 가 다루는 것은 역할이 다르다.
 *
 *   StatComponent   최대 체력·공격력처럼 **계산되는** 값. 서버에만 있고 복제되지 않는다
 *   AttributeSet    현재 체력처럼 **시시각각 변하는** 값 + 계산 결과를 받아 적은 최대치
 *
 * 최대치를 여기에도 두는 이유는 둘이다.
 *  - Health 를 클램프하려면 같은 곳에 있어야 한다
 *  - 클라이언트가 체력바를 그리려면 최대치를 알아야 하는데 StatComponent 는 복제되지 않는다
 *
 * 최대치는 GAS 모디파이어가 아니라 SetNumericAttributeBase 로 덮어쓴다(D8).
 * GAS 기본 모디파이어로는 (Base+Added) × (1+ΣInc) × Π(1+More) 를 표현할 수 없다.
 */
UCLASS()
class TD_PROJECT_API UTDAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UTDAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 값이 실제로 바뀌기 직전에 불린다. 여기서 범위를 강제한다. */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// ── 체력 ──────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "TD|Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UTDAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "TD|Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UTDAttributeSet, MaxHealth)

	// ── 마나 ──────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mana, Category = "TD|Attributes")
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(UTDAttributeSet, Mana)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxMana, Category = "TD|Attributes")
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(UTDAttributeSet, MaxMana)

	// ── 데미지 수신 (메타 어트리뷰트) ─────────────────────

	/**
	 * 이번 피격의 데미지가 잠시 담기는 우편함.
	 * 복제하지 않는다 — 서버가 PostGameplayEffectExecute 에서 소비하고 바로 비우며,
	 * 클라이언트에는 결과인 Health 만 가면 된다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Attributes|Meta")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UTDAttributeSet, IncomingDamage)

	/** GameplayEffect 실행 직후(서버 전용). IncomingDamage 를 Health 에 반영한다. */
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
	
protected:
	// OnRep 은 GAMEPLAYATTRIBUTE_REPNOTIFY 를 불러야 예측 보정이 올바르게 동작한다.

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldValue);
};
