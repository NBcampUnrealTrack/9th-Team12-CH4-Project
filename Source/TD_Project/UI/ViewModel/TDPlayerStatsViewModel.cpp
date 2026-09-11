#include "UI/ViewModel/TDPlayerStatsViewModel.h"
#include "Abilities/TDAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Core/TDGameplayTags.h"
#include "Player/TDPlayerState.h"
#include "Stats/TDProgressionComponent.h"
#include "Character/TDCharacterClassData.h"
#include "Data/TDCharacterClassRow.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Settings/TDCharacterClassSettings.h"

namespace
{
 TArray<FGameplayAttribute> VitalAttributes()
 {
  return {UTDAttributeSet::GetHealthAttribute(), UTDAttributeSet::GetMaxHealthAttribute(),
   UTDAttributeSet::GetManaAttribute(), UTDAttributeSet::GetMaxManaAttribute()};
 }
 FText VitalText(float Current, float Maximum)
 {
  return FText::Format(NSLOCTEXT("TDPlayerStatus", "VitalValue", "{0} / {1}"),
   FText::AsNumber(FMath::RoundToInt(Current)), FText::AsNumber(FMath::RoundToInt(Maximum)));
 }
}

void UTDPlayerStatsViewModel::SetSource(ATDPlayerState* InPlayerState)
{
 if (InPlayerState && Source.Get() == InPlayerState) return;
 UnbindSource();
 Source = InPlayerState;
 if (InPlayerState)
 {
  // 이름은 변경을 감시하지 않고 최초 연결과 캐릭터 선택 완료 시 읽는다.
  InPlayerState->OnCharacterSelected.AddUniqueDynamic(this, &ThisClass::RefreshIdentity);
  InPlayerState->OnCharacterClassChanged.AddUniqueDynamic(this, &ThisClass::RefreshClassVisuals);
  InPlayerState->OnStatsReplicated.AddUniqueDynamic(this, &ThisClass::RefreshCombatStats);
  InPlayerState->OnCombatPowerChanged.AddUniqueDynamic(this, &ThisClass::HandleCombatPowerChanged);
  BoundASC = InPlayerState->GetAbilitySystemComponent();
  if (UAbilitySystemComponent* ASC = BoundASC.Get())
  {
   for (const FGameplayAttribute& Attribute : VitalAttributes())
    AttributeHandles.Add(ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
     .AddUObject(this, &ThisClass::HandleAttributeChanged));
  }
  BoundProgression = InPlayerState->GetProgressionComponent();
  if (UTDProgressionComponent* Progression = BoundProgression.Get())
   Progression->OnProgressionChanged.AddUniqueDynamic(this, &ThisClass::RefreshProgression);
 }
 UE_MVVM_SET_PROPERTY_VALUE(HasPlayerState, InPlayerState != nullptr);
 RefreshAll();
}

void UTDPlayerStatsViewModel::RefreshAll()
{
 ATDPlayerState* Player = Source.Get();
 RefreshClassVisuals(Player ? Player->GetCharacterClassId() : NAME_None);
 RefreshIdentity();
 RefreshProgression();
 RefreshVitals();
 RefreshCombatStats();
}

void UTDPlayerStatsViewModel::UnbindSource()
{
 if (ATDPlayerState* PS = Source.Get())
 {
  PS->OnCharacterSelected.RemoveDynamic(this, &ThisClass::RefreshIdentity);
  PS->OnCharacterClassChanged.RemoveDynamic(this, &ThisClass::RefreshClassVisuals);
  PS->OnStatsReplicated.RemoveDynamic(this, &ThisClass::RefreshCombatStats);
  PS->OnCombatPowerChanged.RemoveDynamic(this, &ThisClass::HandleCombatPowerChanged);
 }
 if (UTDProgressionComponent* Progression = BoundProgression.Get())
  Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::RefreshProgression);
 if (UAbilitySystemComponent* ASC = BoundASC.Get())
 {
  const TArray<FGameplayAttribute> Attributes = VitalAttributes();
  for (int32 Index = 0; Index < AttributeHandles.Num(); ++Index)
   ASC->GetGameplayAttributeValueChangeDelegate(Attributes[Index]).Remove(AttributeHandles[Index]);
 }
 AttributeHandles.Reset();
 BoundASC.Reset();
 BoundProgression.Reset();
 Source.Reset();
}

void UTDPlayerStatsViewModel::BeginDestroy()
{
 UnbindSource();
 Super::BeginDestroy();
}

void UTDPlayerStatsViewModel::RefreshIdentity()
{
 const ATDPlayerState* PS = Source.Get();
 const FText NewName = PS ? FText::FromString(PS->GetPlayerName()) : FText::GetEmpty();
 if (!PlayerName.EqualTo(NewName)) UE_MVVM_SET_PROPERTY_VALUE(PlayerName, NewName);
 const FText NewLabel = PS
  ? FText::Format(NSLOCTEXT("TDPlayerStatus", "LevelAndName", "Lv.{0}   {1}"), FText::AsNumber(Level), PlayerName)
  : NSLOCTEXT("TDPlayerStatus", "Waiting", "캐릭터 대기 중");
 if (!LevelNameText.EqualTo(NewLabel)) UE_MVVM_SET_PROPERTY_VALUE(LevelNameText, NewLabel);
}

void UTDPlayerStatsViewModel::RefreshProgression()
{
 const UTDProgressionComponent* Progression = BoundProgression.Get();
 UE_MVVM_SET_PROPERTY_VALUE(Level, Progression ? Progression->GetLevel() : 1);
 UE_MVVM_SET_PROPERTY_VALUE(TotalExp, Progression ? Progression->GetExp() : 0);
 UE_MVVM_SET_PROPERTY_VALUE(ExpToNextLevel, Progression ? Progression->GetExpToNextLevel() : 0);
 UE_MVVM_SET_PROPERTY_VALUE(ExpPercent, Progression ? Progression->GetLevelProgress() : 0.f);
 RefreshIdentity();
}

void UTDPlayerStatsViewModel::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
 RefreshVitals();
}

void UTDPlayerStatsViewModel::RefreshVitals()
{
 const UAbilitySystemComponent* ASC = BoundASC.Get();
 UE_MVVM_SET_PROPERTY_VALUE(Health, ASC ? ASC->GetNumericAttribute(UTDAttributeSet::GetHealthAttribute()) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, ASC ? ASC->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute()) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(Mana, ASC ? ASC->GetNumericAttribute(UTDAttributeSet::GetManaAttribute()) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(MaxMana, ASC ? ASC->GetNumericAttribute(UTDAttributeSet::GetMaxManaAttribute()) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, MaxHealth > 0.f ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(ManaPercent, MaxMana > 0.f ? FMath::Clamp(Mana / MaxMana, 0.f, 1.f) : 0.f);
 const FText NewHealthText = VitalText(Health, MaxHealth);
 const FText NewManaText = VitalText(Mana, MaxMana);
 if (!HealthText.EqualTo(NewHealthText)) UE_MVVM_SET_PROPERTY_VALUE(HealthText, NewHealthText);
 if (!ManaText.EqualTo(NewManaText)) UE_MVVM_SET_PROPERTY_VALUE(ManaText, NewManaText);
}

void UTDPlayerStatsViewModel::RefreshCombatStats()
{
 const ATDPlayerState* PS = Source.Get();
 // 클라이언트에서는 서버가 복제한 최종 스탯만 읽는다.
 UE_MVVM_SET_PROPERTY_VALUE(PhysicalAttack, PS ? PS->GetReplicatedStat(TDTags::Stat_Offense_Damage_Physical) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(MagicalAttack, PS ? PS->GetReplicatedStat(TDTags::Stat_Offense_Damage_Magical) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(Defense, PS ? PS->GetReplicatedStat(TDTags::Stat_Defense_Armor) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(CriticalChance, PS ? PS->GetReplicatedStat(TDTags::Stat_Offense_CritChance) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(CriticalDamage, PS ? PS->GetReplicatedStat(TDTags::Stat_Offense_CritDamage) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(ArmorPenetration, PS ? PS->GetReplicatedStat(TDTags::Stat_Offense_ArmorPenetration) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(BossDamage, PS ? PS->GetReplicatedStat(TDTags::Stat_Offense_BossDamage) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(DamageReduction, PS ? PS->GetReplicatedStat(TDTags::Stat_Defense_DamageReduction) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(HealthRegen, PS ? PS->GetReplicatedStat(TDTags::Stat_Resource_Health_Regen) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(ManaRegen, PS ? PS->GetReplicatedStat(TDTags::Stat_Resource_Mana_Regen) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(MoveSpeed, PS ? PS->GetReplicatedStat(TDTags::Stat_Utility_MoveSpeed) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(CooldownRecoveryRate, PS ? PS->GetReplicatedStat(TDTags::Stat_Utility_CooldownRecoveryRate) : 0.f);
 UE_MVVM_SET_PROPERTY_VALUE(CombatPower, PS ? PS->GetCombatPower() : 0);
}

void UTDPlayerStatsViewModel::HandleCombatPowerChanged(int32 NewValue)
{
 UE_MVVM_SET_PROPERTY_VALUE(CombatPower, NewValue);
}

void UTDPlayerStatsViewModel::RefreshClassVisuals(FName NewClassId)
{
 // 직업이 바뀌거나 이미지가 없으면 이전 캐릭터의 그림을 남기지 않는다.
 FText NewClassName = NewClassId.IsNone()
  ? NSLOCTEXT("TDPlayerStatus", "ClassUnselected", "미선택") : FText::FromName(NewClassId);
 UTexture2D* NewPortrait = nullptr;
 UTexture2D* NewIcon = nullptr;
 UTexture2D* NewFullBody = nullptr;
 if (!NewClassId.IsNone())
 {
  const UDataTable* Table = UTDCharacterClassSettings::Get()->ClassTable.LoadSynchronous();
  const FTDCharacterClassRow* Row = Table
   ? Table->FindRow<FTDCharacterClassRow>(NewClassId, TEXT("PlayerClassVisuals"), false) : nullptr;
  if (Row)
  {
   if (!Row->DisplayName.IsEmpty()) NewClassName = Row->DisplayName;
   if (const UTDCharacterClassData* Visuals = Row->VisualData.LoadSynchronous())
   {
    NewPortrait = Visuals->Portrait.LoadSynchronous();
    NewIcon = Visuals->Icon.LoadSynchronous();
    NewFullBody = Visuals->FullBody.LoadSynchronous();
   }
  }
 }
 if (!CharacterClassName.EqualTo(NewClassName)) UE_MVVM_SET_PROPERTY_VALUE(CharacterClassName, NewClassName);
 UE_MVVM_SET_PROPERTY_VALUE(CharacterPortrait, NewPortrait);
 UE_MVVM_SET_PROPERTY_VALUE(CharacterClassIcon, NewIcon);
 UE_MVVM_SET_PROPERTY_VALUE(CharacterFullBody, NewFullBody);
}