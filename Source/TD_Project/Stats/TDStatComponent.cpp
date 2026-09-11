#include "Stats/TDStatComponent.h"

#include "Core/TDGameplayTags.h"
#include "Data/TDStatRow.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Stats/TDStatCalculation.h"

UTDStatComponent::UTDStatComponent()
{
	// 스탯 재계산은 Tick이 아니라 더티 플래그와 이벤트로 처리한다.
	PrimaryComponentTick.bCanEverTick = false;
}

void UTDStatComponent::BeginPlay()
{
	Super::BeginPlay();

	LoadStatDefinitions();
}

void UTDStatComponent::LoadStatDefinitions()
{
	if (StatTable == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: StatTable 이 지정되지 않아 기본값과 클램프가 적용되지 않는다."),
			*GetNameSafe(GetOwner()));
		return;
	}

	TArray<FTDStatRow*> Rows;
	StatTable->GetAllRows<FTDStatRow>(TEXT("UTDStatComponent::LoadStatDefinitions"), Rows);

	StatRanges.Reserve(Rows.Num());

	for (const FTDStatRow* Row : Rows)
	{
		if (Row == nullptr || !Row->StatTag.IsValid())
		{
			continue;
		}

		BaseValues.Add(Row->StatTag, Row->DefaultValue);

		// 범위는 플래그가 켜진 쪽만 좁힌다. 값만 보고 판단하면 "상한 0" 인 스탯과
		// "상한 없음" 을 구별할 수 없다.
		FStatRange& Range = StatRanges.FindOrAdd(Row->StatTag);
		if (Row->bHasMinValue)
		{
			Range.Min = Row->MinValue;
		}
		if (Row->bHasMaxValue)
		{
			Range.Max = Row->MaxValue;
		}
	}

	InvalidateCache();
	NotifyStatsChanged();
}

bool UTDStatComponent::HasAuthorityToModify() const
{
	const AActor* Owner = GetOwner();
	return Owner != nullptr && Owner->HasAuthority();
}

FTDStatSourceHandle UTDStatComponent::AddSource(FGameplayTag SourceTag, TArray<FTDStatModifier> Modifiers)
{
	if (!HasAuthorityToModify())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTDStatComponent::AddSource 는 서버에서만 호출해야 한다. SourceTag=%s"),
			*SourceTag.ToString());
		return FTDStatSourceHandle();
	}

	const FTDStatSourceHandle Handle(NextSourceId++);

	FSource& Source = Sources.Add(Handle);
	Source.SourceTag = SourceTag;
	Source.Modifiers = MoveTemp(Modifiers);

	RebuildFlatModifiers();
	InvalidateCache();
	NotifyStatsChanged();

	return Handle;
}

bool UTDStatComponent::RemoveSource(const FTDStatSourceHandle& Handle)
{
	if (!HasAuthorityToModify())
	{
		UE_LOG(LogTemp, Warning, TEXT("UTDStatComponent::RemoveSource 는 서버에서만 호출해야 한다."));
		return false;
	}

	if (Sources.Remove(Handle) == 0)
	{
		return false;
	}

	RebuildFlatModifiers();
	InvalidateCache();
	NotifyStatsChanged();

	return true;
}

FTDStatSourceHandle UTDStatComponent::AddTimedSource(FGameplayTag SourceTag,
	TArray<FTDStatModifier> Modifiers, float Duration)
{
	if (Duration <= 0.f)
	{
		return FTDStatSourceHandle();
	}

	const FTDStatSourceHandle Handle = AddSource(SourceTag, MoveTemp(Modifiers));
	if (!Handle.IsValid())
	{
		return Handle;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		// 타이머를 걸 곳이 없으면 영영 안 걷힌다. 붙이지 않는 편이 낫다.
		RemoveSource(Handle);
		return FTDStatSourceHandle();
	}

	// 핸들마다 타이머가 하나씩 생긴다. 버프가 동시에 여럿 걸릴 수 있어 핸들 하나로는 모자란다.
	// 걷히면 맵에서도 지워지므로 쌓이지 않는다.
	FTimerHandle& Timer = TimedSourceTimers.Add(Handle);

	World->GetTimerManager().SetTimer(Timer,
		FTimerDelegate::CreateWeakLambda(this, [this, Handle]()
		{
			RemoveSource(Handle);
			TimedSourceTimers.Remove(Handle);
		}),
		Duration, /*bLoop=*/false);

	return Handle;
}

FTDStatSourceHandle UTDStatComponent::ReplaceSource(const FTDStatSourceHandle& OldHandle,
	FGameplayTag SourceTag, TArray<FTDStatModifier> Modifiers)
{
	if (!HasAuthorityToModify())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UTDStatComponent::ReplaceSource 는 서버에서만 호출해야 한다. SourceTag=%s"),
			*SourceTag.ToString());
		return FTDStatSourceHandle();
	}

	// 제거와 등록 사이에 알림을 내지 않는 것이 이 함수의 존재 이유다.
	if (OldHandle.IsValid())
	{
		Sources.Remove(OldHandle);
	}

	FTDStatSourceHandle NewHandle;
	if (Modifiers.Num() > 0)
	{
		NewHandle = FTDStatSourceHandle(NextSourceId++);

		FSource& Source = Sources.Add(NewHandle);
		Source.SourceTag = SourceTag;
		Source.Modifiers = MoveTemp(Modifiers);
	}

	RebuildFlatModifiers();
	InvalidateCache();
	NotifyStatsChanged();

	return NewHandle;
}

void UTDStatComponent::ClearSources()
{
	if (!HasAuthorityToModify())
	{
		UE_LOG(LogTemp, Warning, TEXT("UTDStatComponent::ClearSources 는 서버에서만 호출해야 한다."));
		return;
	}

	if (Sources.IsEmpty())
	{
		return;
	}

	Sources.Reset();
	FlatModifiers.Reset();
	InvalidateCache();
	NotifyStatsChanged();
}

void UTDStatComponent::SetBaseValue(FGameplayTag Stat, float Value)
{
	if (!Stat.IsValid())
	{
		return;
	}

	float& Stored = BaseValues.FindOrAdd(Stat);
	if (Stored == Value)
	{
		return;
	}

	Stored = Value;
	InvalidateCache();
	NotifyStatsChanged();
}

float UTDStatComponent::GetBaseValue(FGameplayTag Stat) const
{
	const float* Found = BaseValues.Find(Stat);
	return Found ? *Found : 0.f;
}

TArray<FGameplayTag> UTDStatComponent::GetDefinedStats() const
{
	// StatRanges 는 LoadStatDefinitions 에서만 채워지므로 테이블에 있는 행과 정확히 일치한다.
	// BaseValues 는 SetBaseValue 로도 늘어날 수 있어 "테이블에 정의된 것"과 어긋날 수 있다.
	TArray<FGameplayTag> Result;
	StatRanges.GetKeys(Result);

	return Result;
}

float UTDStatComponent::GetStat(FGameplayTag Stat) const
{
	if (!bCacheValid)
	{
		CachedStats.Reset();
		bCacheValid = true;
	}

	if (const float* Cached = CachedStats.Find(Stat))
	{
		return *Cached;
	}

	const float Value = ComputeStat(Stat, FGameplayTagContainer::EmptyContainer);
	CachedStats.Add(Stat, Value);

	return Value;
}

float UTDStatComponent::GetStatWithContext(FGameplayTag Stat, const FGameplayTagContainer& Context) const
{
	// 컨텍스트가 비어 있으면 조건부는 어차피 전부 제외된다. 캐시 경로와 결과가 같으므로 그쪽을 쓴다.
	if (Context.IsEmpty())
	{
		return GetStat(Stat);
	}

	return ComputeStat(Stat, Context);
}

float UTDStatComponent::ComputeStat(FGameplayTag Stat, const FGameplayTagContainer& Context) const
{
	// 범위가 등록돼 있지 않으면 제한 없이 계산한다. 테이블에 행이 없는 스탯도
	// 모디파이어만으로 값을 가질 수 있으므로 오류로 취급하지 않는다.
	const FStatRange* Range = StatRanges.Find(Stat);
	const float MinValue = Range ? Range->Min : -TNumericLimits<float>::Max();
	const float MaxValue = Range ? Range->Max : TNumericLimits<float>::Max();

	return TDStat::Calculate(Stat, GetBaseValue(Stat), FlatModifiers, Context, MinValue, MaxValue);
}

void UTDStatComponent::RebuildFlatModifiers()
{
	int32 TotalCount = 0;
	for (const TPair<FTDStatSourceHandle, FSource>& Pair : Sources)
	{
		TotalCount += Pair.Value.Modifiers.Num();
	}

	FlatModifiers.Reset(TotalCount);
	for (const TPair<FTDStatSourceHandle, FSource>& Pair : Sources)
	{
		FlatModifiers.Append(Pair.Value.Modifiers);
	}
}

float UTDStatComponent::GetCombatPower() const
{
	if (bCombatPowerValid)
	{
		return CachedCombatPower;
	}

	// 물리·마법 중 높은 쪽을 주력으로 본다. 직업으로 가르는 편이 정확하지만
	// 큰 쪽을 고르면 하이브리드 직업이 생겨도 그대로 동작한다.
	const float PhysicalDamage = GetStat(TDTags::Stat_Offense_Damage_Physical);
	const float MagicalDamage = GetStat(TDTags::Stat_Offense_Damage_Magical);
	const float MainDamage = FMath::Max(PhysicalDamage, MagicalDamage);

	// CritDamage 는 총 배율이다(기본 1.5 = 150%). 기대값이므로 초과분만 확률로 곱한다.
	const float CritChance = FMath::Clamp(GetStat(TDTags::Stat_Offense_CritChance), 0.f, 1.f);
	const float CritDamage = GetStat(TDTags::Stat_Offense_CritDamage);
	const float CritMultiplier = 1.f + CritChance * FMath::Max(0.f, CritDamage - 1.f);

	const float BossMultiplier = 1.f + FMath::Max(0.f, GetStat(TDTags::Stat_Offense_BossDamage));

	CachedCombatPower = MainDamage * CritMultiplier * BossMultiplier;
	bCombatPowerValid = true;

	return CachedCombatPower;
}

void UTDStatComponent::InvalidateCache()
{
	bCacheValid = false;
	bCombatPowerValid = false;
}

void UTDStatComponent::NotifyStatsChanged()
{
	OnStatsChanged.Broadcast();
}
