#include "Stats/TDProgressionComponent.h"

#include "Core/TDGameplayTags.h"
#include "Data/TDClassGrowthRow.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Stats/TDStatComponent.h"

namespace
{
	const TCHAR* ClassGrowthContext = TEXT("UTDProgressionComponent");

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

	// 저장된 데이터를 읽어오는 경우에는 ReadSaveData 가 다시 호출한다.
	// 새 캐릭터는 여기서 레벨 1 성장이 적용된다.
	RefreshStatModifiers();
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

	Level = ClampedLevel;
	RefreshStatModifiers();

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

	return true;
}

void UTDProgressionComponent::AddExp(int32 Amount)
{
	if (!HasAuthorityToModify() || Amount <= 0)
	{
		return;
	}

	Exp += Amount;

	// TODO: 필요 경험치 곡선 테이블이 생기면 여기서 레벨업을 판정하고 SetLevel 을 부른다.
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
	Level = FMath::Max(1, In.Level);
	Exp = FMath::Max(0, In.Exp);
	SkillLevels = In.SkillLevels;

	RefreshStatModifiers();
}
