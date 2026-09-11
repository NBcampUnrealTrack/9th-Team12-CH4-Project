#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "TDPlayerStatsViewModel.generated.h"

class UTexture2D;
class ATDPlayerState;
class UAbilitySystemComponent;
class UTDProgressionComponent;
struct FOnAttributeChangeData;

/** 같은 로컬 플레이어의 HUD, EXP, 스탯창이 공유하는 읽기 전용 표시 데이터. */
UCLASS(BlueprintType, meta=(MVVMAllowedContextCreationType="Manual"))
class TD_PROJECT_API UTDPlayerStatsViewModel : public UMVVMViewModelBase
{
 GENERATED_BODY()
public:
 void SetSource(ATDPlayerState* InPlayerState);
 UFUNCTION(BlueprintCallable, Category="TD|UI|ViewModel")
 void RefreshAll();

 /** HUD와 스탯창에서 공통으로 읽는 직업 표시 정보. */
 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Character Visuals")
 FText CharacterClassName;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Character Visuals")
 TObjectPtr<UTexture2D> CharacterPortrait = nullptr;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Character Visuals")
 TObjectPtr<UTexture2D> CharacterFullBody = nullptr;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Character Visuals")
 TObjectPtr<UTexture2D> CharacterClassIcon = nullptr;
 virtual void BeginDestroy() override;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 FText PlayerName = FText::GetEmpty();

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 int32 Level = 1;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float Health = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float MaxHealth = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float Mana = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float MaxMana = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float HealthPercent = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float ManaPercent = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 FText LevelNameText = FText::GetEmpty();

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 FText HealthText = FText::GetEmpty();

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 FText ManaText = FText::GetEmpty();

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 int32 TotalExp = 0;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 int32 ExpToNextLevel = 0;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float ExpPercent = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float PhysicalAttack = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float MagicalAttack = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float Defense = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float CriticalChance = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float CriticalDamage = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float ArmorPenetration = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float BossDamage = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float DamageReduction = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float HealthRegen = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float ManaRegen = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float MoveSpeed = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 float CooldownRecoveryRate = 0.f;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 int32 CombatPower = 0;

 UPROPERTY(BlueprintReadOnly, FieldNotify, Category="TD|Player Stats")
 bool HasPlayerState = false;

private:
 TWeakObjectPtr<ATDPlayerState> Source;
 TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
 TWeakObjectPtr<UTDProgressionComponent> BoundProgression;
 TArray<FDelegateHandle> AttributeHandles;
 void UnbindSource();
 void RefreshVitals();
 void HandleAttributeChanged(const FOnAttributeChangeData& Data);

 UFUNCTION()
 void RefreshIdentity();
 UFUNCTION()
 void RefreshClassVisuals(FName NewClassId);
 UFUNCTION()
 void RefreshProgression();
 UFUNCTION()
 void RefreshCombatStats();
 UFUNCTION()
 void HandleCombatPowerChanged(int32 NewValue);
};
