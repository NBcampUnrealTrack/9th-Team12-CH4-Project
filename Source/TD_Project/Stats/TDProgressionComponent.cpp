#include "Stats/TDProgressionComponent.h"

#include "Core/TDGameplayTags.h"
#include "Data/TDClassGrowthRow.h"
#include "Data/TDLevelExpRow.h"
#include "Data/TDSkillPassiveRow.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Game/TDGameMode.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Stats/TDStatComponent.h"

namespace
{
	const TCHAR* ClassGrowthContext = TEXT("UTDProgressionComponent");
	const TCHAR* LevelExpContext = TEXT("UTDProgressionComponent::LevelExp");
	const TCHAR* SkillContext = TEXT("UTDProgressionComponent::Skill");
	const TCHAR* SkillPassiveContext = TEXT("UTDProgressionComponent::SkillPassive");
	const TCHAR* SkillEffectContext = TEXT("UTDProgressionComponent::SkillEffect");

	/** 직업과 무관하게 모두에게 적용되는 성장 행의 ClassId. */
	const FName DefaultClassId(TEXT("Default"));
}

UTDProgressionComponent::UTDProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTDProgressionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 남의 레벨도 파티 창이나 이름표에 보여야 하므로 조건을 걸지 않는다.
	DOREPLIFETIME(UTDProgressionComponent, Level);
	DOREPLIFETIME(UTDProgressionComponent, Exp);
	DOREPLIFETIME(UTDProgressionComponent, ClassId);
	DOREPLIFETIME(UTDProgressionComponent, SkillLevels);
}

void UTDProgressionComponent::BeginPlay()
{
	Super::BeginPlay();

	// 지정 누락은 조용히 실패한다 — 레벨을 올려도 스탯이 그대로여서 원인을 찾기 어렵다.
	if (ClassGrowthTable == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: ProgressionComponent 의 ClassGrowthTable(DT_ClassGrowth) 이 지정되지 않았다. "
				 "레벨 성장이 전혀 적용되지 않는다."),
			*GetNameSafe(GetOwner()));
	}

	if (LevelExpTable == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: ProgressionComponent 의 LevelExpTable(DT_LevelExp) 이 지정되지 않았다. "
				 "경험치가 쌓이기만 하고 레벨이 오르지 않는다."),
			*GetNameSafe(GetOwner()));
	}

	if (SkillTable == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: ProgressionComponent 의 SkillTable(DT_Skill) 이 지정되지 않았다. "
				 "스킬을 찍을 수 없다."),
			*GetNameSafe(GetOwner()));
	}

	if (SkillPassiveTable == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: ProgressionComponent 의 SkillPassiveTable(DT_SkillPassive) 이 지정되지 않았다. "
				 "패시브를 찍어도 스탯이 오르지 않는다."),
			*GetNameSafe(GetOwner()));
	}

	if (SkillEffectTable == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: ProgressionComponent 의 SkillEffectTable(DT_SkillEffect) 이 지정되지 않았다. "
				 "액티브가 시전은 되지만 아무 일도 일어나지 않는다."),
			*GetNameSafe(GetOwner()));
	}

	// 저장된 데이터를 읽어오는 경우에는 ReadSaveData 가 다시 호출한다.
	// 새 캐릭터는 여기서 레벨 1 성장이 적용된다.
	RefreshStatModifiers();
	RefreshSkillModifiers();
}

bool UTDProgressionComponent::HasAuthorityToModify() const
{
	const AActor* Owner = GetOwner();
	return Owner != nullptr && Owner->HasAuthority();
}

UTDStatComponent* UTDProgressionComponent::FindStatComponent() const
{
	// PlayerState 로 캐스팅하지 않는다. 같은 액터에 붙어 있기만 하면 되므로
	// 나중에 몬스터에 붙이더라도 그대로 동작한다.
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UTDStatComponent>() : nullptr;
}

bool UTDProgressionComponent::SetLevel(int32 NewLevel)
{
	if (!HasAuthorityToModify())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SetLevel: 서버 권한이 없다. 클라이언트에서는 레벨을 바꿀 수 없다."));
		return false;
	}

	const int32 ClampedLevel = FMath::Max(1, NewLevel);
	if (Level == ClampedLevel)
	{
		return true;
	}

	// 경험치를 그 레벨의 시작점으로 맞춘다. 이걸 빼먹으면 다음에 경험치를 받는 순간
	// 옛 경험치 기준으로 레벨이 다시 계산되어 방금 지정한 값이 사라진다.
	Exp = GetRequiredExpForLevel(ClampedLevel);

	ApplyLevelChange(ClampedLevel);
	OnProgressionChanged.Broadcast();

	return true;
}

bool UTDProgressionComponent::SetClassId(FName NewClassId)
{
	if (!HasAuthorityToModify())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SetClassId: 서버 권한이 없다. 클라이언트에서는 직업을 바꿀 수 없다."));
		return false;
	}

	if (ClassId == NewClassId)
	{
		return true;
	}

	ClassId = NewClassId;
	RefreshStatModifiers();

	// 직업이 바뀌면 쓸 수 있는 스킬이 통째로 달라진다. 이전 직업의 패시브가
	// 그대로 남아 있으면 전사가 마법사 패시브를 달고 다니게 된다.
	RefreshSkillModifiers();

	return true;
}

void UTDProgressionComponent::AddExp(int32 Amount)
{
	if (!HasAuthorityToModify() || Amount <= 0)
	{
		return;
	}

	Exp += Amount;

		// 레벨은 저장하지 않는 파생값이다. 경험치가 늘 때마다 다시 구한다.
	ApplyLevelChange(CalculateLevelFromExp(Exp));

	// 경험치바처럼 값을 그리는 UI 가 구독한다.
	OnProgressionChanged.Broadcast();

	// 획득 알림은 이 플레이어에게만 간다. 위 델리게이트와 하는 일이 다르다 —
	// 그쪽은 화면의 숫자를 갱신하고, 이쪽은 채팅창에 한 줄을 남긴다.
	const APlayerState* OwnerState = Cast<APlayerState>(GetOwner());
	APlayerController* Controller = OwnerState ? OwnerState->GetPlayerController() : nullptr;

	if (Controller != nullptr)
	{
		if (ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr)
		{
			GameMode->SendSystemMessage(Controller, ETDChatChannel::Loot,
				FString::Printf(TEXT("경험치 %d 획득"), Amount));
		}
	}
}

int32 UTDProgressionComponent::CalculateLevelFromExp(int32 TotalExp) const
{
	if (LevelExpTable == nullptr)
	{
		return Level;
	}

	TArray<FTDLevelExpRow*> Rows;
	LevelExpTable->GetAllRows<FTDLevelExpRow>(LevelExpContext, Rows);

	// 요구치를 넘긴 행 중 가장 높은 레벨을 고른다.
	// 행 순서를 믿지 않는 이유는 시트에서 정렬이 흐트러질 수 있기 때문이다.
	int32 Result = 1;
	for (const FTDLevelExpRow* Row : Rows)
	{
		if (Row != nullptr && TotalExp >= Row->RequiredTotalExp && Row->Level > Result)
		{
			Result = Row->Level;
		}
	}

	return Result;
}

int32 UTDProgressionComponent::GetRequiredExpForLevel(int32 InLevel) const
{
	if (LevelExpTable == nullptr)
	{
		return 0;
	}

	TArray<FTDLevelExpRow*> Rows;
	LevelExpTable->GetAllRows<FTDLevelExpRow>(LevelExpContext, Rows);

	for (const FTDLevelExpRow* Row : Rows)
	{
		if (Row != nullptr && Row->Level == InLevel)
		{
			return Row->RequiredTotalExp;
		}
	}

	return 0;
}

int32 UTDProgressionComponent::GetMaxLevel() const
{
	if (LevelExpTable == nullptr)
	{
		return 1;
	}

	TArray<FTDLevelExpRow*> Rows;
	LevelExpTable->GetAllRows<FTDLevelExpRow>(LevelExpContext, Rows);

	int32 Result = 1;
	for (const FTDLevelExpRow* Row : Rows)
	{
		if (Row != nullptr && Row->Level > Result)
		{
			Result = Row->Level;
		}
	}

	return Result;
}

int32 UTDProgressionComponent::GetExpSpanForLevel(int32 InLevel) const
{
	if (InLevel < 1 || InLevel >= GetMaxLevel())
	{
		return 0;
	}

	return FMath::Max(0, GetRequiredExpForLevel(InLevel + 1) - GetRequiredExpForLevel(InLevel));
}

int32 UTDProgressionComponent::GetExpToNextLevel() const
{
	if (Level >= GetMaxLevel())
	{
		return 0;
	}

	return FMath::Max(0, GetRequiredExpForLevel(Level + 1) - Exp);
}

float UTDProgressionComponent::GetLevelProgress() const
{
	if (Level >= GetMaxLevel())
	{
		return 1.f;
	}

	const int32 CurrentBase = GetRequiredExpForLevel(Level);
	const int32 NextBase = GetRequiredExpForLevel(Level + 1);

	const int32 Span = NextBase - CurrentBase;
	if (Span <= 0)
	{
		// 곡선이 잘못 입력돼 다음 레벨 요구치가 더 낮거나 같은 경우다.
		return 0.f;
	}

	return FMath::Clamp(static_cast<float>(Exp - CurrentBase) / Span, 0.f, 1.f);
}

void UTDProgressionComponent::ApplyLevelChange(int32 NewLevel)
{
	const int32 ClampedLevel = FMath::Max(1, NewLevel);
	if (Level == ClampedLevel)
	{
		return;
	}

	const int32 PreviousLevel = Level;
	Level = ClampedLevel;

	// 성장 모디파이어가 새 레벨 기준으로 다시 만들어진다.
	RefreshStatModifiers();

	// 서버에서는 OnRep 이 없으므로 직접 알린다.
	OnLevelUp.Broadcast(Level, PreviousLevel);

	UE_LOG(LogTemp, Log, TEXT("%s — 레벨 %d → %d (누적 경험치 %d)"),
		*GetNameSafe(GetOwner()), PreviousLevel, Level, Exp);
}

int32 UTDProgressionComponent::GetRemainingSkillPoints() const
{
	int32 UsedPoints = 0;
	for (const FTDSkillLevel& Skill : SkillLevels)
	{
		UsedPoints += Skill.Level;
	}

	return (Level * SkillPointsPerLevel) - UsedPoints;
}

int32 UTDProgressionComponent::GetSkillLevel(FName SkillId) const
{
	const FTDSkillLevel* Found = SkillLevels.FindByPredicate(
		[SkillId](const FTDSkillLevel& Skill) { return Skill.SkillId == SkillId; });

	return Found ? Found->Level : 0;
}

// ── 스킬 ──────────────────────────────────────────────────

const FTDSkillRow* UTDProgressionComponent::FindSkillRow(FName SkillId) const
{
	if (SkillTable == nullptr || SkillId.IsNone())
	{
		return nullptr;
	}

	// 없는 행을 묻는 것이 정상인 경로가 있다(스킬창이 잘못된 ID 를 보낸 경우 등).
	// 경고를 끄고 nullptr 로 판단하게 한다.
	return SkillTable->FindRow<FTDSkillRow>(SkillId, SkillContext, /*bWarnIfRowMissing=*/false);
}

bool UTDProgressionComponent::GetSkillInfo(FName SkillId, FTDSkillRow& OutRow) const
{
	if (const FTDSkillRow* Row = FindSkillRow(SkillId))
	{
		OutRow = *Row;
		return true;
	}

	return false;
}

TArray<FName> UTDProgressionComponent::GetClassSkills() const
{
	TArray<FName> Result;
	if (SkillTable == nullptr || ClassId.IsNone())
	{
		return Result;
	}

	for (const FName& RowName : SkillTable->GetRowNames())
	{
		const FTDSkillRow* Row = SkillTable->FindRow<FTDSkillRow>(RowName, SkillContext, false);
		if (Row != nullptr && Row->ClassId == ClassId)
		{
			Result.Add(RowName);
		}
	}

	return Result;
}

FName UTDProgressionComponent::GetSkillForSlot(int32 SlotIndex) const
{
	if (SkillTable == nullptr || ClassId.IsNone() || SlotIndex <= 0)
	{
		return NAME_None;
	}

	for (const FName& RowName : SkillTable->GetRowNames())
	{
		const FTDSkillRow* Row = SkillTable->FindRow<FTDSkillRow>(RowName, SkillContext, false);
		if (Row != nullptr
			&& Row->ClassId == ClassId
			&& Row->SkillType == ETDSkillType::Active
			&& Row->SlotIndex == SlotIndex)
		{
			return RowName;
		}
	}

	return NAME_None;
}

TArray<FTDSkillEffectRow> UTDProgressionComponent::GetSkillEffects(FName SkillId) const
{
	TArray<FTDSkillEffectRow> Result;
	if (SkillEffectTable == nullptr || SkillId.IsNone())
	{
		return Result;
	}

	TArray<FTDSkillEffectRow*> Rows;
	SkillEffectTable->GetAllRows<FTDSkillEffectRow>(SkillEffectContext, Rows);

	for (const FTDSkillEffectRow* Row : Rows)
	{
		if (Row != nullptr && Row->SkillId == SkillId)
		{
			Result.Add(*Row);
		}
	}

	return Result;
}

bool UTDProgressionComponent::CanUpgradeSkill(FName SkillId) const
{
	const FTDSkillRow* Row = FindSkillRow(SkillId);
	if (Row == nullptr)
	{
		return false;
	}

	// 남의 직업 스킬은 찍을 수 없다.
	if (Row->ClassId != ClassId)
	{
		return false;
	}

	// 선행 스킬 조건은 두지 않는다(Q10). 캐릭터 레벨 하나뿐이라 스킬창이 목록으로 끝난다.
	if (Level < Row->RequiredLevel)
	{
		return false;
	}

	if (GetSkillLevel(SkillId) >= Row->MaxLevel)
	{
		return false;
	}

	return GetRemainingSkillPoints() > 0;
}

void UTDProgressionComponent::ServerUpgradeSkill_Implementation(FName SkillId)
{
	if (!HasAuthorityToModify())
	{
		return;
	}

	if (!CanUpgradeSkill(SkillId))
	{
		// 화면이 낡았거나 위조된 요청이다. 정상적인 UI 라면 버튼이 이미 회색이어야 한다.
		UE_LOG(LogTemp, Warning,
			TEXT("%s: 스킬 '%s' 를 올릴 수 없다. (직업·레벨·최대치·잔여 포인트 확인)"),
			*GetNameSafe(GetOwner()), *SkillId.ToString());
		return;
	}

	if (FTDSkillLevel* Found = SkillLevels.FindByPredicate(
		[SkillId](const FTDSkillLevel& Skill) { return Skill.SkillId == SkillId; }))
	{
		++Found->Level;
	}
	else
	{
		SkillLevels.Emplace(SkillId, 1);
	}

	// 액티브를 찍어도 부르는 이유는, 그 스킬이 패시브인지 여기서 판별하지 않기 때문이다.
	// 어차피 묶음을 통째로 다시 만들므로 액티브뿐이면 결과가 같다.
	RefreshSkillModifiers();

	// 서버에서는 OnRep 이 불리지 않으므로 직접 알린다.
	OnProgressionChanged.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("%s — 스킬 '%s' 레벨 %d (잔여 포인트 %d)"),
		*GetNameSafe(GetOwner()), *SkillId.ToString(),
		GetSkillLevel(SkillId), GetRemainingSkillPoints());
}

#if !UE_BUILD_SHIPPING
int32 UTDProgressionComponent::DebugLearnAllSkills(int32 SkillLevel)
{
	if (!HasAuthorityToModify())
	{
		return 0;
	}

	int32 ChangedCount = 0;

	for (const FName& SkillId : GetClassSkills())
	{
		const FTDSkillRow* Row = FindSkillRow(SkillId);
		if (Row == nullptr)
		{
			continue;
		}

		const int32 TargetLevel = FMath::Clamp(SkillLevel, 0, Row->MaxLevel);
		if (GetSkillLevel(SkillId) == TargetLevel)
		{
			continue;
		}

		if (FTDSkillLevel* Found = SkillLevels.FindByPredicate(
			[SkillId](const FTDSkillLevel& Skill) { return Skill.SkillId == SkillId; }))
		{
			Found->Level = TargetLevel;
		}
		else if (TargetLevel > 0)
		{
			SkillLevels.Emplace(SkillId, TargetLevel);
		}

		++ChangedCount;
	}

	if (ChangedCount > 0)
	{
		RefreshSkillModifiers();
		OnProgressionChanged.Broadcast();
	}

	return ChangedCount;
}
#endif // !UE_BUILD_SHIPPING

void UTDProgressionComponent::RefreshSkillModifiers()
{
	if (!HasAuthorityToModify())
	{
		return;
	}

	UTDStatComponent* StatComponent = FindStatComponent();
	if (StatComponent == nullptr || SkillPassiveTable == nullptr)
	{
		return;
	}

	TArray<FTDSkillPassiveRow*> Rows;
	SkillPassiveTable->GetAllRows<FTDSkillPassiveRow>(SkillPassiveContext, Rows);

	TArray<FTDStatModifier> Modifiers;

	for (const FTDSkillPassiveRow* Row : Rows)
	{
		if (Row == nullptr || !Row->StatTag.IsValid())
		{
			continue;
		}

		const int32 SkillLevel = GetSkillLevel(Row->SkillId);
		if (SkillLevel <= 0)
		{
			// 안 찍은 스킬이다. 테이블에는 모든 직업의 패시브가 들어 있으므로 대부분 여기서 걸린다.
			continue;
		}

		// 다른 직업의 패시브가 SkillLevels 에 남아 있어도 붙지 않게 한다.
		// 정상적으로는 ServerUpgradeSkill 이 막지만, 직업이 다른 세이브를 읽으면 새어 들어온다.
		const FTDSkillRow* Definition = FindSkillRow(Row->SkillId);
		if (Definition == nullptr || Definition->ClassId != ClassId)
		{
			continue;
		}

		const float Value = Row->BaseValue + Row->ValuePerLevel * (SkillLevel - 1);
		if (FMath::IsNearlyZero(Value))
		{
			continue;
		}

		Modifiers.Emplace(Row->StatTag, Row->Op, Value);
	}

	// 성장과 마찬가지로 묶음을 통째로 갈아 끼운다. 제거와 등록을 따로 부르면
	// 그 사이에 "패시브가 없는 순간" 이 알림으로 새어 나간다.
	SkillStatSourceHandle = StatComponent->ReplaceSource(
		SkillStatSourceHandle, TDTags::Source_Skill, MoveTemp(Modifiers));
}

void UTDProgressionComponent::RefreshStatModifiers()
{
	// 모디파이어 등록은 서버 몫이다. 클라이언트가 시도하면 스탯 컴포넌트가 거부하면서
	// 경고만 남기므로 여기서 먼저 걸러낸다.
	if (!HasAuthorityToModify())
	{
		return;
	}

	UTDStatComponent* StatComponent = FindStatComponent();
	if (StatComponent == nullptr || ClassGrowthTable == nullptr)
	{
		return;
	}

	TArray<FTDClassGrowthRow*> Rows;
	ClassGrowthTable->GetAllRows<FTDClassGrowthRow>(ClassGrowthContext, Rows);

	TArray<FTDStatModifier> Modifiers;
	Modifiers.Reserve(Rows.Num());

	for (const FTDClassGrowthRow* Row : Rows)
	{
		if (Row == nullptr || !Row->StatTag.IsValid() || FMath::IsNearlyZero(Row->ValuePerLevel))
		{
			continue;
		}

		// Default 행은 모두에게, 나머지는 자기 직업 행만 적용된다.
		// 공통 성장을 직업마다 반복해서 적지 않아도 되게 하려는 것.
		const bool bApplies = (Row->ClassId == DefaultClassId) || (!ClassId.IsNone() && Row->ClassId == ClassId);
		if (!bApplies)
		{
			continue;
		}

		Modifiers.Emplace(Row->StatTag, ETDModOp::Added, Row->ValuePerLevel * Level);
	}

	// 직업을 지정했는데 그 ClassId 의 행이 하나도 없으면 오타이거나 테이블에 안 넣은 것이다.
	// Default 성장만 적용된 채로 조용히 넘어가면 알아채기 어렵다.
	if (!ClassId.IsNone())
	{
		const bool bFoundClassRow = Rows.ContainsByPredicate(
			[this](const FTDClassGrowthRow* Row) { return Row != nullptr && Row->ClassId == ClassId; });

		if (!bFoundClassRow)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("DT_ClassGrowth 에 ClassId '%s' 인 행이 없다. Default 성장만 적용된다."),
				*ClassId.ToString());
		}
	}

	// 이전에 등록한 묶음을 새 것으로 갈아 끼운다. 지워지는 것은 계산용 모디파이어이고,
	// 원본인 Level 과 ClassId 는 그대로 남는다.
	//
	// 제거와 등록을 따로 부르면 그 사이에 "성장 모디파이어가 없는 순간" 이 알림으로 새어 나가,
	// 그 상태를 받은 쪽이 최대 체력을 기본값으로 착각한다.
	StatSourceHandle = StatComponent->ReplaceSource(
		StatSourceHandle, TDTags::Source_Progression, MoveTemp(Modifiers));
}

void UTDProgressionComponent::WriteSaveData(FTDPlayerSaveData& Out) const
{
	// 값을 옮겨 담기만 한다. 어디에 어떻게 저장할지는 저장 담당자가 정한다.
	Out.ClassId = ClassId;
	Out.Level = Level;
	Out.Exp = Exp;
	Out.SkillLevels = SkillLevels;

	// HealthRatio / ManaRatio 는 채우지 않는다. 현재 체력은 AttributeSet 이 관리하는
	// 런타임 상태이며, GAS 를 붙이는 S4 에서 그쪽이 채운다.
}

void UTDProgressionComponent::ReadSaveData(const FTDPlayerSaveData& In)
{
	// TODO: In.SaveVersion 이 늘어나면 여기서 옛 형식을 변환한다.

	ClassId = In.ClassId;
	Exp = FMath::Max(0, In.Exp);
	SkillLevels = In.SkillLevels;

	// 저장된 Level 은 쓰지 않고 경험치로부터 다시 구한다.
	// 곡선을 조정했다면 기존 캐릭터도 새 기준으로 자동 보정된다(D12).
	// 테이블이 아직 없으면 저장값을 그대로 살려 데이터를 잃지 않는다.
	Level = FMath::Max(1, In.Level);
	if (LevelExpTable != nullptr)
	{
		Level = CalculateLevelFromExp(Exp);
	}

	RefreshStatModifiers();
	RefreshSkillModifiers();
	OnProgressionChanged.Broadcast();
}

void UTDProgressionComponent::OnRep_Level(int32 PreviousLevel)
{
	OnLevelUp.Broadcast(Level, PreviousLevel);
	OnProgressionChanged.Broadcast();
}

void UTDProgressionComponent::OnRep_Exp()
{
	OnProgressionChanged.Broadcast();
}

void UTDProgressionComponent::OnRep_SkillLevels()
{
	// 스킬창의 레벨 표시와 잔여 포인트가 함께 바뀐다. 경험치바도 같은 신호를 받지만
	// 값이 그대로라 다시 그려도 화면은 같다.
	OnProgressionChanged.Broadcast();
}
