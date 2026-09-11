#include "Quest/TDQuestComponent.h"

#include "Core/TDGameplayTags.h"
#include "Data/TDItemRow.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Items/TDInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/TDPlayerState.h"
#include "Quest/TDPersonalWorldStateComponent.h"
#include "Stats/TDProgressionComponent.h"

namespace
{
	const TCHAR* QuestTableContext = TEXT("UTDQuestComponent");

	bool IsMainQuest(const FTDQuestRow& Definition)
	{
		return Definition.QuestTypeTag ==
			TDTags::Quest_Type_Main.GetTag();
	}

	bool IsSubQuest(const FTDQuestRow& Definition)
	{
		return Definition.QuestTypeTag ==
			TDTags::Quest_Type_Sub.GetTag();
	}

	bool IsActiveState(const FGameplayTag StateTag)
	{
		return StateTag ==
				TDTags::Quest_State_Active.GetTag()
			|| StateTag ==
				TDTags::Quest_State_ReadyToTurnIn.GetTag();
	}
}

UTDQuestComponent::UTDQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTDQuestComponent::BeginPlay()
{
	Super::BeginPlay();

	if (QuestTable == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: QuestComponent의 QuestTable(DT_Quest)이 지정되지 않았다."),
			*GetNameSafe(GetOwner()));
		return;
	}

	ATDPlayerState* PlayerState =
		Cast<ATDPlayerState>(GetOwner());

	if (PlayerState == nullptr)
	{
		return;
	}

	if (UTDInventoryComponent* Inventory =
		PlayerState->GetInventoryComponent())
	{
		Inventory->OnInventoryChanged.AddUniqueDynamic(
			this,
			&UTDQuestComponent::HandleInventoryChanged);
	}

	PlayerState->OnZoneChanged.AddUniqueDynamic(
		this,
		&UTDQuestComponent::HandleZoneChanged);

	PlayerState->OnCharacterSelected.AddUniqueDynamic(
		this,
		&UTDQuestComponent::HandleCharacterSelected);

	if (PlayerState->HasAuthority()
		&& PlayerState->HasSelectedCharacter())
	{
		HandleCharacterSelected();
	}
}

void UTDQuestComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(
		UTDQuestComponent,
		QuestEntries,
		COND_OwnerOnly);

	DOREPLIFETIME_CONDITION(
		UTDQuestComponent,
		AffectionEntries,
		COND_OwnerOnly);
}

int32 UTDQuestComponent::GetCurrentKstDayKey()
{
	const FDateTime KoreaNow =
		FDateTime::UtcNow()
		+ FTimespan::FromHours(9.0);

	return KoreaNow.GetYear() * 10000
		+ KoreaNow.GetMonth() * 100
		+ KoreaNow.GetDay();
}

const FTDQuestRow* UTDQuestComponent::FindQuestDefinition(
	FName QuestId) const
{
	if (QuestTable == nullptr || QuestId.IsNone())
	{
		return nullptr;
	}

	return QuestTable->FindRow<FTDQuestRow>(
		QuestId,
		QuestTableContext,
		false);
}

const FTDQuestRow* UTDQuestComponent::GetQuestDefinition(
	FName QuestId) const
{
	return FindQuestDefinition(QuestId);
}

FTDQuestRuntimeData* UTDQuestComponent::FindMutableQuest(
	FName QuestId)
{
	return QuestEntries.FindByPredicate(
		[QuestId](const FTDQuestRuntimeData& Entry)
		{
			return Entry.QuestId == QuestId;
		});
}

const FTDQuestRuntimeData* UTDQuestComponent::FindQuest(
	FName QuestId) const
{
	return QuestEntries.FindByPredicate(
		[QuestId](const FTDQuestRuntimeData& Entry)
		{
			return Entry.QuestId == QuestId;
		});
}

FTDAffectionRuntimeData*
UTDQuestComponent::FindMutableAffection(FName NPCId)
{
	return AffectionEntries.FindByPredicate(
		[NPCId](const FTDAffectionRuntimeData& Entry)
		{
			return Entry.NPCId == NPCId;
		});
}

const FTDAffectionRuntimeData*
UTDQuestComponent::FindAffection(FName NPCId) const
{
	return AffectionEntries.FindByPredicate(
		[NPCId](const FTDAffectionRuntimeData& Entry)
		{
			return Entry.NPCId == NPCId;
		});
}

bool UTDQuestComponent::HasQuest(FName QuestId) const
{
	return FindQuest(QuestId) != nullptr;
}

bool UTDQuestComponent::HasCompletedQuest(
	FName QuestId) const
{
	const FTDQuestRuntimeData* Entry =
		FindQuest(QuestId);

	return Entry != nullptr
		&& Entry->StateTag ==
			TDTags::Quest_State_Completed.GetTag();
}

FGameplayTag UTDQuestComponent::GetQuestStateTag(
	FName QuestId) const
{
	const FTDQuestRuntimeData* Entry =
		FindQuest(QuestId);

	return Entry
		? Entry->StateTag
		: FGameplayTag();
}

int32 UTDQuestComponent::GetActiveSubQuestCount() const
{
	int32 Count = 0;

	for (const FTDQuestRuntimeData& Entry : QuestEntries)
	{
		if (!IsActiveState(Entry.StateTag))
		{
			continue;
		}

		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition != nullptr
			&& IsSubQuest(*Definition))
		{
			++Count;
		}
	}

	return Count;
}

int32 UTDQuestComponent::GetAffectionPoints(
	FName NPCId) const
{
	const FTDAffectionRuntimeData* Entry =
		FindAffection(NPCId);

	return Entry ? Entry->Points : 0;
}

FText UTDQuestComponent::GetAffectionTierText(
	FName NPCId) const
{
	const int32 Points =
		GetAffectionPoints(NPCId);

	if (Points >= 100)
	{
		return NSLOCTEXT(
			"TDAffection",
			"Max",
			"호감 MAX");
	}

	if (Points >= 80)
	{
		return NSLOCTEXT(
			"TDAffection",
			"Special",
			"특별한 사이");
	}

	if (Points >= 60)
	{
		return NSLOCTEXT(
			"TDAffection",
			"Trust",
			"신뢰");
	}

	if (Points >= 40)
	{
		return NSLOCTEXT(
			"TDAffection",
			"Friendly",
			"친밀");
	}

	if (Points >= 20)
	{
		return NSLOCTEXT(
			"TDAffection",
			"Acquaintance",
			"아는 사이");
	}

	return NSLOCTEXT(
		"TDAffection",
		"Stranger",
		"낯선 사이");
}

bool UTDQuestComponent::AddAffection(
	FName NPCId,
	int32 Amount)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority()
		|| NPCId.IsNone()
		|| Amount < 0)
	{
		return false;
	}

	FTDAffectionRuntimeData* Entry =
		FindMutableAffection(NPCId);

	if (Entry == nullptr)
	{
		FTDAffectionRuntimeData& NewEntry =
			AffectionEntries.AddDefaulted_GetRef();

		NewEntry.NPCId = NPCId;
		NewEntry.Points = 0;
		NewEntry.LastGiftKstDayKey = 0;

		Entry = &NewEntry;
	}

	if (Amount > 0)
	{
		const int64 NewPoints =
			static_cast<int64>(Entry->Points)
			+ Amount;

		Entry->Points = static_cast<int32>(
			FMath::Min<int64>(
				NewPoints,
				MAX_int32));
	}

	OnAffectionChanged.Broadcast(
		NPCId,
		Entry->Points);

	OwnerActor->ForceNetUpdate();
	return true;
}

ETDQuestActionResult
UTDQuestComponent::CheckAcceptConditions(
	FName QuestId,
	bool bIgnoreSubQuestLimit) const
{
	const FTDQuestRow* Definition =
		FindQuestDefinition(QuestId);

	if (Definition == nullptr)
	{
		return ETDQuestActionResult::InvalidDefinition;
	}

	const FTDQuestRuntimeData* Existing =
		FindQuest(QuestId);

	if (Existing != nullptr)
	{
		if (IsActiveState(Existing->StateTag))
		{
			return ETDQuestActionResult::AlreadyAccepted;
		}

		if (Existing->StateTag ==
			TDTags::Quest_State_Completed.GetTag())
		{
			if (Definition->RepeatType ==
				ETDQuestRepeatType::None)
			{
				return ETDQuestActionResult::AlreadyCompleted;
			}

			if (Existing->CompletedKstDayKey ==
				GetCurrentKstDayKey())
			{
				return ETDQuestActionResult::DailyAlreadyCompleted;
			}
		}
	}

	const ATDPlayerState* PlayerState =
		Cast<ATDPlayerState>(GetOwner());

	if (PlayerState == nullptr)
	{
		return ETDQuestActionResult::InvalidDefinition;
	}

	if (IsMainQuest(*Definition))
	{
		for (const FTDQuestRuntimeData& Other :
			QuestEntries)
		{
			if (Other.QuestId == QuestId
				|| !IsActiveState(Other.StateTag))
			{
				continue;
			}

			const FTDQuestRow* OtherDefinition =
				FindQuestDefinition(Other.QuestId);

			if (OtherDefinition != nullptr
				&& IsMainQuest(*OtherDefinition))
			{
				return ETDQuestActionResult::
					MainQuestAlreadyActive;
			}
		}
	}
	else if (IsSubQuest(*Definition)
		&& !bIgnoreSubQuestLimit
		&& GetActiveSubQuestCount() >= 2)
	{
		return ETDQuestActionResult::
			ActiveSubQuestLimit;
	}

	for (const FName PrerequisiteId :
		Definition->PrerequisiteQuestIds)
	{
		if (PrerequisiteId.IsNone()
			|| !HasCompletedQuest(PrerequisiteId))
		{
			return ETDQuestActionResult::
				PrerequisiteNotMet;
		}
	}

	const UTDProgressionComponent* Progression =
		PlayerState->GetProgressionComponent();

	const int32 CurrentLevel =
		Progression ? Progression->GetLevel() : 0;

	if (CurrentLevel < Definition->MinimumLevel)
	{
		return ETDQuestActionResult::
			PrerequisiteNotMet;
	}

	if (!Definition->AllowedClassIds.IsEmpty())
	{
		const FName CurrentClass =
			PlayerState->GetCharacterClassId();

		if (!Definition->AllowedClassIds.Contains(
			CurrentClass))
		{
			return ETDQuestActionResult::
				PrerequisiteNotMet;
		}
	}

	for (const FTDQuestAffectionRequirement& Requirement :
		Definition->AffectionRequirements)
	{
		if (Requirement.NPCId.IsNone()
			|| GetAffectionPoints(Requirement.NPCId)
				< Requirement.RequiredPoints)
		{
			return ETDQuestActionResult::
				PrerequisiteNotMet;
		}
	}

	const UTDPersonalWorldStateComponent* PersonalState =
		PlayerState->GetPersonalWorldStateComponent();

	if (PersonalState == nullptr
		|| !PersonalState->MatchesCondition(
			Definition->AcceptCondition))
	{
		return ETDQuestActionResult::
			PrerequisiteNotMet;
	}

	return ETDQuestActionResult::Success;
}

ETDQuestActionResult
UTDQuestComponent::GetQuestAcceptResult(
	FName QuestId,
	bool bIgnoreSubQuestLimit) const
{
	return CheckAcceptConditions(
		QuestId,
		bIgnoreSubQuestLimit);
}

void UTDQuestComponent::InitializeObjectiveProgress(
	FTDQuestRuntimeData& Entry,
	const FTDQuestRow& Definition) const
{
	Entry.ObjectiveProgress.Init(
		0,
		Definition.Objectives.Num());

	const ATDPlayerState* PlayerState =
		Cast<ATDPlayerState>(GetOwner());

	const UTDInventoryComponent* Inventory =
		PlayerState
			? PlayerState->GetInventoryComponent()
			: nullptr;

	const FGameplayTag CurrentZone =
		PlayerState
			? PlayerState->GetCurrentZoneId()
			: FGameplayTag();

	for (int32 Index = 0;
		Index < Definition.Objectives.Num();
		++Index)
	{
		const FTDQuestObjectiveDefinition& Objective =
			Definition.Objectives[Index];

		const int32 Required =
			FMath::Max(1, Objective.RequiredCount);

		if (Objective.ObjectiveType ==
			ETDQuestObjectiveType::OwnItem)
		{
			const int32 CurrentCount =
				Inventory
					? Inventory->GetItemCount(
						Objective.TargetId)
					: 0;

			Entry.ObjectiveProgress[Index] =
				FMath::Clamp(
					CurrentCount,
					0,
					Required);
		}
		else if (Objective.ObjectiveType ==
			ETDQuestObjectiveType::EnterZone)
		{
			Entry.ObjectiveProgress[Index] =
				CurrentZone.IsValid()
				&& CurrentZone ==
					Objective.TargetZone
					? 1
					: 0;
		}
	}
}

bool UTDQuestComponent::IsReadyToTurnIn(
	const FTDQuestRuntimeData& Entry,
	const FTDQuestRow& Definition) const
{
	if (Definition.bWaitForFutureContent)
	{
		return false;
	}

	if (Entry.ObjectiveProgress.Num()
		!= Definition.Objectives.Num())
	{
		return false;
	}

	for (int32 Index = 0;
		Index < Definition.Objectives.Num();
		++Index)
	{
		const int32 Required =
			FMath::Max(
				1,
				Definition.Objectives[Index]
					.RequiredCount);

		if (Entry.ObjectiveProgress[Index]
			< Required)
		{
			return false;
		}
	}

	return true;
}

bool UTDQuestComponent::RefreshQuestState(
	FTDQuestRuntimeData& Entry,
	const FTDQuestRow& Definition)
{
	if (Entry.StateTag ==
		TDTags::Quest_State_Completed.GetTag())
	{
		return false;
	}

	const FGameplayTag NewState =
		IsReadyToTurnIn(Entry, Definition)
			? TDTags::Quest_State_ReadyToTurnIn.GetTag()
			: TDTags::Quest_State_Active.GetTag();

	if (Entry.StateTag == NewState)
	{
		return false;
	}

	Entry.StateTag = NewState;
	ApplyQuestStateTags(Entry, Definition);
	return true;
}

void UTDQuestComponent::RemoveQuestStateTags(
	const FTDQuestRow& Definition)
{
	ATDPlayerState* PlayerState =
		Cast<ATDPlayerState>(GetOwner());

	UTDPersonalWorldStateComponent* PersonalState =
		PlayerState
			? PlayerState
				->GetPersonalWorldStateComponent()
			: nullptr;

	if (PersonalState == nullptr)
	{
		return;
	}

	PersonalState->RemoveQuestTag(
		Definition.AcceptedTag);

	PersonalState->RemoveQuestTag(
		Definition.ReadyTag);

	PersonalState->RemoveQuestTag(
		Definition.CompletedTag);
}

void UTDQuestComponent::ApplyQuestStateTags(
	const FTDQuestRuntimeData& Entry,
	const FTDQuestRow& Definition)
{
	ATDPlayerState* PlayerState =
		Cast<ATDPlayerState>(GetOwner());

	UTDPersonalWorldStateComponent* PersonalState =
		PlayerState
			? PlayerState
				->GetPersonalWorldStateComponent()
			: nullptr;

	if (PersonalState == nullptr)
	{
		return;
	}

	RemoveQuestStateTags(Definition);

	if (Entry.StateTag ==
		TDTags::Quest_State_Active.GetTag())
	{
		PersonalState->AddQuestTag(
			Definition.AcceptedTag);
	}
	else if (Entry.StateTag ==
		TDTags::Quest_State_ReadyToTurnIn.GetTag())
	{
		PersonalState->AddQuestTag(
			Definition.ReadyTag);
	}
	else if (Entry.StateTag ==
		TDTags::Quest_State_Completed.GetTag())
	{
		PersonalState->AddQuestTag(
			Definition.CompletedTag);
	}
}

ETDQuestActionResult UTDQuestComponent::AcceptQuest(
	FName QuestId)
{
	return AcceptQuestInternal(
		QuestId,
		false,
		ETDQuestTargetType::None,
		NAME_None);
}

ETDQuestActionResult
UTDQuestComponent::AcceptQuestAtTarget(
	FName QuestId,
	ETDQuestTargetType TargetType,
	FName TargetId)
{
	return AcceptQuestInternal(
		QuestId,
		true,
		TargetType,
		TargetId);
}

ETDQuestActionResult
UTDQuestComponent::AcceptQuestInternal(
	FName QuestId,
	bool bValidateTarget,
	ETDQuestTargetType TargetType,
	FName TargetId)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority())
	{
		return ETDQuestActionResult::
			InvalidDefinition;
	}

	const FTDQuestRow* Definition =
		FindQuestDefinition(QuestId);

	if (Definition == nullptr)
	{
		return ETDQuestActionResult::
			InvalidDefinition;
	}

	if (bValidateTarget
		&& (Definition->AcceptTargetType != TargetType
			|| Definition->AcceptTargetId != TargetId))
	{
		return ETDQuestActionResult::
			WrongAcceptTarget;
	}

	const ETDQuestActionResult CheckResult =
		CheckAcceptConditions(
			QuestId,
			false);

	if (CheckResult !=
		ETDQuestActionResult::Success)
	{
		return CheckResult;
	}

	FTDQuestRuntimeData* Entry =
		FindMutableQuest(QuestId);

	if (Entry == nullptr)
	{
		FTDQuestRuntimeData& NewEntry =
			QuestEntries.AddDefaulted_GetRef();

		NewEntry.QuestId = QuestId;
		Entry = &NewEntry;
	}

	Entry->StateTag =
		TDTags::Quest_State_Active.GetTag();

	Entry->CompletedKstDayKey = 0;
	Entry->AcceptSequence =
		NextAcceptSequence++;

	InitializeObjectiveProgress(
		*Entry,
		*Definition);

	RefreshQuestState(
		*Entry,
		*Definition);

	ApplyQuestStateTags(
		*Entry,
		*Definition);

	NotifyQuestListChanged();

	UE_LOG(LogTemp, Log,
		TEXT("퀘스트 수락: Player='%s', Quest='%s'"),
		*GetNameSafe(OwnerActor),
		*QuestId.ToString());

	if (Definition->bAutoCompleteWithoutTurnIn
		&& Entry->StateTag ==
			TDTags::Quest_State_ReadyToTurnIn.GetTag())
	{
		TurnInQuest(QuestId);
	}

	return ETDQuestActionResult::Success;
}

void UTDQuestComponent::ServerAbandonQuest_Implementation(
	FName QuestId)
{
	AbandonQuest(QuestId);
}

ETDQuestActionResult UTDQuestComponent::AbandonQuest(FName QuestId)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
	{
		return ETDQuestActionResult::InvalidDefinition;
	}

	const int32 Index =
		QuestEntries.IndexOfByPredicate(
			[QuestId](const FTDQuestRuntimeData& Entry)
			{
				return Entry.QuestId == QuestId;
			});

	if (!QuestEntries.IsValidIndex(Index))
	{
		return ETDQuestActionResult::NotActive;
	}

	const FTDQuestRow* Definition = FindQuestDefinition(QuestId);

	if (Definition == nullptr)
	{
		return ETDQuestActionResult::InvalidDefinition;
	}

	if (IsMainQuest(*Definition))
	{
		return ETDQuestActionResult::CannotAbandonMain;
	}

	// 메인이 아니라는 이유만으로 허용하지 않고,
	// 정확히 서브 타입인지 확인한다.
	if (!IsSubQuest(*Definition))
	{
		return ETDQuestActionResult::InvalidDefinition;
	}

	if (!IsActiveState(QuestEntries[Index].StateTag))
	{
		return ETDQuestActionResult::NotActive;
	}

	RemoveQuestStateTags(*Definition);
	QuestEntries.RemoveAt(Index);

	// HUD, 퀘스트 창, NPC 표시 등이 기존 변경 알림을 받는다.
	NotifyQuestListChanged();

	return ETDQuestActionResult::Success;
}

int32 UTDQuestComponent::ReportQuestEvent(
	FGameplayTag EventTag,
	int32 Amount)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority()
		|| !EventTag.IsValid()
		|| Amount <= 0)
	{
		return 0;
	}

	int32 ChangedQuestCount = 0;

	for (FTDQuestRuntimeData& Entry :
		QuestEntries)
	{
		if (!IsActiveState(Entry.StateTag))
		{
			continue;
		}

		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition == nullptr)
		{
			continue;
		}

		bool bChanged = false;

		for (int32 Index = 0;
			Index < Definition->Objectives.Num();
			++Index)
		{
			const FTDQuestObjectiveDefinition& Objective =
				Definition->Objectives[Index];

			if (Objective.ObjectiveType !=
					ETDQuestObjectiveType::GameplayEvent
				|| !Objective.EventTag.IsValid()
				|| !EventTag.MatchesTag(
					Objective.EventTag))
			{
				continue;
			}

			const int32 Required =
				FMath::Max(
					1,
					Objective.RequiredCount);

			const int32 Previous =
				Entry.ObjectiveProgress[Index];

			Entry.ObjectiveProgress[Index] =
				FMath::Clamp(
					Previous + Amount,
					0,
					Required);

			bChanged |=
				Entry.ObjectiveProgress[Index]
				!= Previous;
		}

		if (bChanged)
		{
			RefreshQuestState(
				Entry,
				*Definition);

			++ChangedQuestCount;
		}
	}

	if (ChangedQuestCount > 0)
	{
		NotifyQuestListChanged();
		ProcessAutomaticQuests();
	}

	return ChangedQuestCount;
}

ETDQuestActionResult UTDQuestComponent::ReportQuestEventForQuest(
	FName QuestId,
	FGameplayTag EventTag,
	int32 Amount)
{
	AActor* OwnerActor = GetOwner();

	// 퀘스트 진행도는 서버에서만 변경한다.
	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority()
		|| QuestId.IsNone()
		|| !EventTag.IsValid()
		|| Amount <= 0)
	{
		return ETDQuestActionResult::InvalidDefinition;
	}

	const FTDQuestRow* Definition =
		FindQuestDefinition(QuestId);

	if (Definition == nullptr)
	{
		return ETDQuestActionResult::InvalidDefinition;
	}

	// 모든 퀘스트를 순회하지 않고,
	// 전달받은 QuestId에 해당하는 퀘스트 하나만 찾는다.
	FTDQuestRuntimeData* Entry =
		FindMutableQuest(QuestId);

	if (Entry == nullptr)
	{
		return ETDQuestActionResult::NotActive;
	}

	if (Entry->StateTag ==
		TDTags::Quest_State_Completed.GetTag())
	{
		return ETDQuestActionResult::AlreadyCompleted;
	}

	// 진행 중 또는 완료 보고 대기 상태만 허용한다.
	if (!IsActiveState(Entry->StateTag))
	{
		return ETDQuestActionResult::NotActive;
	}

	// 저장된 진행도와 현재 목표 데이터의 개수가 다르면
	// 잘못된 배열 접근을 하지 않고 설정 오류로 처리한다.
	if (Entry->ObjectiveProgress.Num()
		!= Definition->Objectives.Num())
	{
		return ETDQuestActionResult::InvalidDefinition;
	}

	bool bMatchedObjective = false;
	bool bChanged = false;

	for (int32 ObjectiveIndex = 0;
		ObjectiveIndex < Definition->Objectives.Num();
		++ObjectiveIndex)
	{
		const FTDQuestObjectiveDefinition& Objective =
			Definition->Objectives[ObjectiveIndex];

		if (Objective.ObjectiveType !=
				ETDQuestObjectiveType::GameplayEvent
			|| !Objective.EventTag.IsValid()
			|| !EventTag.MatchesTag(Objective.EventTag))
		{
			continue;
		}

		bMatchedObjective = true;

		const int32 RequiredCount =
			FMath::Max(1, Objective.RequiredCount);

		const int32 PreviousCount =
			Entry->ObjectiveProgress[ObjectiveIndex];

		// 계산 중 정수 범위를 넘지 않도록 큰 정수로 더한 뒤,
		// 최종 진행도는 0 ~ 목표 수량 사이로 제한한다.
		const int64 AddedCount =
			static_cast<int64>(PreviousCount)
			+ static_cast<int64>(Amount);

		const int32 NewCount =
			static_cast<int32>(
				FMath::Clamp<int64>(
					AddedCount,
					static_cast<int64>(0),
					static_cast<int64>(RequiredCount)));

		if (NewCount != PreviousCount)
		{
			Entry->ObjectiveProgress[ObjectiveIndex] =
				NewCount;

			bChanged = true;
		}
	}

	// QuestId는 맞지만 그 퀘스트에 해당 이벤트 목표가 없으면
	// 조용히 성공시키지 않고 데이터 설정 오류로 처리한다.
	if (!bMatchedObjective)
	{
		return ETDQuestActionResult::InvalidDefinition;
	}

	// 목표 달성 여부를 다시 계산한다.
	// 상태가 바뀌면 Accepted / Ready 태그도 기존 방식으로 갱신된다.
	bChanged |= RefreshQuestState(
		*Entry,
		*Definition);

	if (bChanged)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("퀘스트 지정 이벤트: Quest='%s', Event='%s'"),
			*QuestId.ToString(),
			*EventTag.ToString());

		NotifyQuestListChanged();
		ProcessAutomaticQuests();
	}

	// 이미 목표 수량에 도달했더라도 올바른 이벤트이면 성공이다.
	// 인벤토리 부족으로 완료 보고에 실패한 뒤 재시도할 수 있게 한다.
	return ETDQuestActionResult::Success;
}

int32 UTDQuestComponent::ReportMonsterKilled(
	FName MonsterId)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority()
		|| MonsterId.IsNone())
	{
		return 0;
	}

	int32 ChangedQuestCount = 0;

	for (FTDQuestRuntimeData& Entry :
		QuestEntries)
	{
		if (!IsActiveState(Entry.StateTag))
		{
			continue;
		}

		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition == nullptr)
		{
			continue;
		}

		bool bChanged = false;

		for (int32 Index = 0;
			Index < Definition->Objectives.Num();
			++Index)
		{
			const FTDQuestObjectiveDefinition& Objective =
				Definition->Objectives[Index];

			if (Objective.ObjectiveType !=
					ETDQuestObjectiveType::KillMonster
				|| Objective.TargetId != MonsterId)
			{
				continue;
			}

			const int32 Required =
				FMath::Max(
					1,
					Objective.RequiredCount);

			const int32 Previous =
				Entry.ObjectiveProgress[Index];

			Entry.ObjectiveProgress[Index] =
				FMath::Min(
					Previous + 1,
					Required);

			bChanged |=
				Entry.ObjectiveProgress[Index]
				!= Previous;
		}

		if (bChanged)
		{
			RefreshQuestState(
				Entry,
				*Definition);

			++ChangedQuestCount;
		}
	}

	if (ChangedQuestCount > 0)
	{
		NotifyQuestListChanged();
		ProcessAutomaticQuests();
	}

	return ChangedQuestCount;
}

int32 UTDQuestComponent::ReportZoneEntered(
	FGameplayTag ZoneId)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority()
		|| !ZoneId.IsValid())
	{
		return 0;
	}

	int32 ChangedQuestCount = 0;

	for (FTDQuestRuntimeData& Entry :
		QuestEntries)
	{
		if (!IsActiveState(Entry.StateTag))
		{
			continue;
		}

		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition == nullptr)
		{
			continue;
		}

		bool bChanged = false;

		for (int32 Index = 0;
			Index < Definition->Objectives.Num();
			++Index)
		{
			const FTDQuestObjectiveDefinition& Objective =
				Definition->Objectives[Index];

			if (Objective.ObjectiveType !=
					ETDQuestObjectiveType::EnterZone
				|| Objective.TargetZone != ZoneId)
			{
				continue;
			}

			if (Entry.ObjectiveProgress[Index] < 1)
			{
				Entry.ObjectiveProgress[Index] = 1;
				bChanged = true;
			}
		}

		if (bChanged)
		{
			RefreshQuestState(
				Entry,
				*Definition);

			++ChangedQuestCount;
		}
	}

	if (ChangedQuestCount > 0)
	{
		NotifyQuestListChanged();
		ProcessAutomaticQuests();
	}

	return ChangedQuestCount;
}

int32 UTDQuestComponent::ReportChestOpened(
	FName ChestId)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority())
	{
		return 0;
	}

	int32 ChangedQuestCount = 0;

	for (FTDQuestRuntimeData& Entry :
		QuestEntries)
	{
		if (!IsActiveState(Entry.StateTag))
		{
			continue;
		}

		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition == nullptr)
		{
			continue;
		}

		bool bChanged = false;

		for (int32 Index = 0;
			Index < Definition->Objectives.Num();
			++Index)
		{
			const FTDQuestObjectiveDefinition& Objective =
				Definition->Objectives[Index];

			if (Objective.ObjectiveType !=
				ETDQuestObjectiveType::OpenChest)
			{
				continue;
			}

			const bool bMatches =
				Objective.TargetId.IsNone()
				|| Objective.TargetId == ChestId;

			if (!bMatches)
			{
				continue;
			}

			const int32 Required =
				FMath::Max(
					1,
					Objective.RequiredCount);

			const int32 Previous =
				Entry.ObjectiveProgress[Index];

			Entry.ObjectiveProgress[Index] =
				FMath::Min(
					Previous + 1,
					Required);

			bChanged |=
				Entry.ObjectiveProgress[Index]
				!= Previous;
		}

		if (bChanged)
		{
			RefreshQuestState(
				Entry,
				*Definition);

			++ChangedQuestCount;
		}
	}

	if (ChangedQuestCount > 0)
	{
		NotifyQuestListChanged();
		ProcessAutomaticQuests();
	}

	return ChangedQuestCount;
}

int32 UTDQuestComponent::RecalculateInventoryObjectives()
{
	if (bApplyingTurnInTransaction)
	{
		return 0;
	}

	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority())
	{
		return 0;
	}

	const ATDPlayerState* PlayerState =
		Cast<ATDPlayerState>(OwnerActor);

	const UTDInventoryComponent* Inventory =
		PlayerState
			? PlayerState->GetInventoryComponent()
			: nullptr;

	if (Inventory == nullptr)
	{
		return 0;
	}

	int32 ChangedQuestCount = 0;

	for (FTDQuestRuntimeData& Entry :
		QuestEntries)
	{
		if (!IsActiveState(Entry.StateTag))
		{
			continue;
		}

		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition == nullptr)
		{
			continue;
		}

		bool bChanged = false;

		for (int32 Index = 0;
			Index < Definition->Objectives.Num();
			++Index)
		{
			const FTDQuestObjectiveDefinition& Objective =
				Definition->Objectives[Index];

			if (Objective.ObjectiveType !=
				ETDQuestObjectiveType::OwnItem)
			{
				continue;
			}

			const int32 Required =
				FMath::Max(
					1,
					Objective.RequiredCount);

			const int32 NewProgress =
				FMath::Clamp(
					Inventory->GetItemCount(
						Objective.TargetId),
					0,
					Required);

			if (Entry.ObjectiveProgress[Index]
				!= NewProgress)
			{
				Entry.ObjectiveProgress[Index] =
					NewProgress;

				bChanged = true;
			}
		}

		bChanged |= RefreshQuestState(
			Entry,
			*Definition);

		if (bChanged)
		{
			++ChangedQuestCount;
		}
	}

	if (ChangedQuestCount > 0)
	{
		NotifyQuestListChanged();
		ProcessAutomaticQuests();
	}

	return ChangedQuestCount;
}

bool UTDQuestComponent::
CanApplyTurnInInventoryTransaction(
	const FTDQuestRow& Definition,
	ETDQuestActionResult& OutFailure) const
{
	OutFailure = ETDQuestActionResult::Success;

	const ATDPlayerState* PlayerState =
		Cast<ATDPlayerState>(GetOwner());

	const UTDInventoryComponent* Inventory =
		PlayerState
			? PlayerState->GetInventoryComponent()
			: nullptr;

	if (Inventory == nullptr)
	{
		OutFailure =
			ETDQuestActionResult::InvalidDefinition;
		return false;
	}

	TArray<FTDItemInstance> SimulatedItems =
		Inventory->GetItems();

	TMap<FName, int32> ItemsToConsume;

	for (const FTDQuestObjectiveDefinition& Objective :
		Definition.Objectives)
	{
		if (Objective.ObjectiveType ==
				ETDQuestObjectiveType::OwnItem
			&& Objective.bConsumeOnTurnIn)
		{
			if (Objective.TargetId.IsNone())
			{
				OutFailure =
					ETDQuestActionResult::
						InvalidDefinition;
				return false;
			}

			ItemsToConsume.FindOrAdd(
				Objective.TargetId)
				+= FMath::Max(
					1,
					Objective.RequiredCount);
		}
	}

	for (const TPair<FName, int32>& Pair :
		ItemsToConsume)
	{
		int32 Remaining = Pair.Value;

		for (FTDItemInstance& Item :
			SimulatedItems)
		{
			if (Item.ItemId != Pair.Key
				|| Remaining <= 0)
			{
				continue;
			}

			const int32 Removed =
				FMath::Min(
					Item.Count,
					Remaining);

			Item.Count -= Removed;
			Remaining -= Removed;
		}

		if (Remaining > 0)
		{
			OutFailure =
				ETDQuestActionResult::
					RequiredItemMissing;
			return false;
		}

		SimulatedItems.RemoveAll(
			[](const FTDItemInstance& Item)
			{
				return Item.Count <= 0;
			});
	}

	for (const FTDQuestItemReward& Reward :
		Definition.ItemRewards)
	{
		if (Reward.ItemId.IsNone()
			|| Reward.Count <= 0)
		{
			OutFailure =
				ETDQuestActionResult::
					InvalidDefinition;
			return false;
		}

		const FTDItemRow* ItemDefinition =
			Inventory->FindItemDefinition(
				Reward.ItemId);

		if (ItemDefinition == nullptr)
		{
			OutFailure =
				ETDQuestActionResult::
					InvalidDefinition;
			return false;
		}

		const int32 MaxStack =
			ItemDefinition->bStackable
				? FMath::Max(
					1,
					ItemDefinition->MaxStackSize)
				: 1;

		int32 Remaining = Reward.Count;

		if (ItemDefinition->bStackable)
		{
			for (FTDItemInstance& Item :
				SimulatedItems)
			{
				if (Item.ItemId != Reward.ItemId
					|| Item.Count >= MaxStack
					|| Remaining <= 0)
				{
					continue;
				}

				const int32 Added =
					FMath::Min(
						Remaining,
						MaxStack - Item.Count);

				Item.Count += Added;
				Remaining -= Added;
			}
		}

		while (Remaining > 0)
		{
			if (SimulatedItems.Num()
				>= Inventory->GetSlotCapacity())
			{
				OutFailure =
					ETDQuestActionResult::
						InventoryFull;
				return false;
			}

			FTDItemInstance NewItem;
			NewItem.ItemId = Reward.ItemId;
			NewItem.Count =
				FMath::Min(
					Remaining,
					MaxStack);

			Remaining -= NewItem.Count;
			SimulatedItems.Add(NewItem);
		}
	}

	return true;
}

bool UTDQuestComponent::
ApplyTurnInInventoryTransaction(
	const FTDQuestRow& Definition)
{
	ATDPlayerState* PlayerState =
		Cast<ATDPlayerState>(GetOwner());

	UTDInventoryComponent* Inventory =
		PlayerState
			? PlayerState->GetInventoryComponent()
			: nullptr;

	if (Inventory == nullptr)
	{
		return false;
	}

	TMap<FName, int32> ItemsToConsume;

	for (const FTDQuestObjectiveDefinition& Objective :
		Definition.Objectives)
	{
		if (Objective.ObjectiveType ==
				ETDQuestObjectiveType::OwnItem
			&& Objective.bConsumeOnTurnIn)
		{
			ItemsToConsume.FindOrAdd(
				Objective.TargetId)
				+= FMath::Max(
					1,
					Objective.RequiredCount);
		}
	}

	bApplyingTurnInTransaction = true;

	for (const TPair<FName, int32>& Pair :
		ItemsToConsume)
	{
		int32 Remaining = Pair.Value;

		while (Remaining > 0)
		{
			int32 FoundSlot = INDEX_NONE;
			int32 FoundCount = 0;

			for (const FTDItemInstance& Item :
				Inventory->GetItems())
			{
				if (Item.ItemId == Pair.Key)
				{
					FoundSlot = Item.SlotIndex;
					FoundCount = Item.Count;
					break;
				}
			}

			if (FoundSlot == INDEX_NONE)
			{
				bApplyingTurnInTransaction = false;
				return false;
			}

			const int32 ToConsume =
				FMath::Min(
					Remaining,
					FoundCount);

			if (!Inventory->ConsumeItemAt(
				FoundSlot,
				ToConsume))
			{
				bApplyingTurnInTransaction = false;
				return false;
			}

			Remaining -= ToConsume;
		}
	}

	for (const FTDQuestItemReward& Reward :
		Definition.ItemRewards)
	{
		if (!Inventory->AddItem(
			Reward.ItemId,
			Reward.Count))
		{
			UE_LOG(LogTemp, Error,
				TEXT("사전 검사 통과 후 퀘스트 보상 지급 실패: Item='%s'"),
				*Reward.ItemId.ToString());

			bApplyingTurnInTransaction = false;
			return false;
		}
	}

	bApplyingTurnInTransaction = false;
	return true;
}

ETDQuestActionResult UTDQuestComponent::TurnInQuest(
	FName QuestId)
{
	return TurnInQuestInternal(
		QuestId,
		false,
		ETDQuestTargetType::None,
		NAME_None);
}

ETDQuestActionResult
UTDQuestComponent::TurnInQuestAtTarget(
	FName QuestId,
	ETDQuestTargetType TargetType,
	FName TargetId)
{
	return TurnInQuestInternal(
		QuestId,
		true,
		TargetType,
		TargetId);
}

ETDQuestActionResult
UTDQuestComponent::TurnInQuestInternal(
	FName QuestId,
	bool bValidateTarget,
	ETDQuestTargetType TargetType,
	FName TargetId)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority())
	{
		return ETDQuestActionResult::
			InvalidDefinition;
	}

	RecalculateInventoryObjectives();

	FTDQuestRuntimeData* Entry =
		FindMutableQuest(QuestId);

	if (Entry == nullptr)
	{
		return ETDQuestActionResult::NotActive;
	}

	if (Entry->StateTag ==
		TDTags::Quest_State_Completed.GetTag())
	{
		return ETDQuestActionResult::
			AlreadyCompleted;
	}

	if (Entry->StateTag !=
		TDTags::Quest_State_ReadyToTurnIn.GetTag())
	{
		return ETDQuestActionResult::NotReady;
	}

	const FTDQuestRow* Definition =
		FindQuestDefinition(QuestId);

	if (Definition == nullptr)
	{
		return ETDQuestActionResult::
			InvalidDefinition;
	}

	if (bValidateTarget
		&& (Definition->TurnInTargetType
				!= TargetType
			|| Definition->TurnInTargetId
				!= TargetId))
	{
		return ETDQuestActionResult::
			WrongTurnInTarget;
	}

	ETDQuestActionResult InventoryFailure;

	if (!CanApplyTurnInInventoryTransaction(
		*Definition,
		InventoryFailure))
	{
		return InventoryFailure;
	}

	ATDPlayerState* PlayerState =
		Cast<ATDPlayerState>(OwnerActor);

	UTDInventoryComponent* Inventory =
		PlayerState
			? PlayerState->GetInventoryComponent()
			: nullptr;

	UTDProgressionComponent* Progression =
		PlayerState
			? PlayerState->GetProgressionComponent()
			: nullptr;

	if ((Definition->GoldReward > 0
			&& Inventory == nullptr)
		|| (Definition->ExpReward > 0
			&& Progression == nullptr))
	{
		return ETDQuestActionResult::
			InvalidDefinition;
	}

	if (!ApplyTurnInInventoryTransaction(
		*Definition))
	{
		return ETDQuestActionResult::
			InvalidDefinition;
	}

	if (Definition->GoldReward > 0)
	{
		Inventory->AddGold(
			Definition->GoldReward);
	}

	if (Definition->ExpReward > 0)
	{
		Progression->AddExp(
			Definition->ExpReward);
	}

	for (const FTDQuestAffectionReward& Reward :
		Definition->AffectionRewards)
	{
		if (!Reward.NPCId.IsNone()
			&& Reward.Amount >= 0)
		{
			AddAffection(
				Reward.NPCId,
				Reward.Amount);
		}
	}

	const bool bWasMainQuest =
		IsMainQuest(*Definition);

	const FName NextQuestId =
		Definition->NextQuestId;

	Entry->StateTag =
		TDTags::Quest_State_Completed.GetTag();

	Entry->CompletedKstDayKey =
		Definition->RepeatType ==
			ETDQuestRepeatType::Cooldown24Hours
				? GetCurrentKstDayKey()
				: 0;

	ApplyQuestStateTags(
		*Entry,
		*Definition);

	// 아이템 제출로 다른 퀘스트의 보유량이 줄 수 있다.
	RecalculateInventoryObjectives();

	NotifyQuestListChanged();

	UE_LOG(LogTemp, Log,
		TEXT("퀘스트 완료: Player='%s', Quest='%s'"),
		*GetNameSafe(OwnerActor),
		*QuestId.ToString());

	/**
	 * 메인만 자동 연계한다.
	 * 서브 연계는 선행 조건만 열리고 NPC/물건에서 직접 받아야 한다.
	 */
	if (bWasMainQuest
		&& !NextQuestId.IsNone())
	{
		const ETDQuestActionResult NextResult =
			AcceptQuest(NextQuestId);

		if (NextResult !=
				ETDQuestActionResult::Success
			&& NextResult !=
				ETDQuestActionResult::AlreadyAccepted)
		{
			UE_LOG(LogTemp, Error,
				TEXT("다음 메인 퀘스트 시작 실패: Current='%s', Next='%s', Result=%d"),
				*QuestId.ToString(),
				*NextQuestId.ToString(),
				static_cast<int32>(NextResult));
		}
	}

	return ETDQuestActionResult::Success;
}

FTDQuestViewData UTDQuestComponent::MakeQuestView(
	const FTDQuestRuntimeData& Entry,
	const FTDQuestRow& Definition) const
{
	FTDQuestViewData View;

	View.QuestId = Entry.QuestId;
	View.QuestTypeTag =
		Definition.QuestTypeTag;
	View.StateTag = Entry.StateTag;
	View.DisplayName =
		Definition.DisplayName;
	View.Description =
		Definition.Description;
	View.ItemRewards =
		Definition.ItemRewards;
	View.ExpReward =
		Definition.ExpReward;
	View.GoldReward =
		Definition.GoldReward;
	View.bDailyQuest =
		Definition.RepeatType ==
			ETDQuestRepeatType::Cooldown24Hours;
	View.AcceptSequence =
		Entry.AcceptSequence;

	for (int32 Index = 0;
		 Index < Definition.Objectives.Num();
		 ++Index)
	{
		const FTDQuestObjectiveDefinition& Objective =
			Definition.Objectives[Index];

		FTDQuestObjectiveView& ObjectiveView =
			View.Objectives.AddDefaulted_GetRef();

		ObjectiveView.Description =
			Objective.Description;

		ObjectiveView.RequiredCount =
			FMath::Max(
				1,
				Objective.RequiredCount);

		ObjectiveView.CurrentCount =
			Entry.ObjectiveProgress.IsValidIndex(Index)
				? Entry.ObjectiveProgress[Index]
				: 0;

		ObjectiveView.bCompleted =
			ObjectiveView.CurrentCount
			>= ObjectiveView.RequiredCount;

		/**
		 * 숫자 진행도는 몬스터 처치와
		 * 현재 아이템 보유 목표에만 표시한다.
		 *
		 * NPC 대화, 지역 진입, 상자 열기는
		 * 목표 설명만 표시한다.
		 */
		ObjectiveView.bShowNumericProgress =
			Objective.ObjectiveType ==
				ETDQuestObjectiveType::KillMonster
			|| Objective.ObjectiveType ==
				ETDQuestObjectiveType::OwnItem;
	}

	return View;
}

TArray<FTDQuestViewData>
UTDQuestComponent::GetQuestViews() const
{
	TArray<FTDQuestViewData> Result;

	for (const FTDQuestRuntimeData& Entry :
		QuestEntries)
	{
		if (!IsActiveState(Entry.StateTag))
		{
			continue;
		}

		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition != nullptr)
		{
			Result.Add(
				MakeQuestView(
					Entry,
					*Definition));
		}
	}

	Result.Sort(
		[](const FTDQuestViewData& A,
		   const FTDQuestViewData& B)
		{
			const bool bAMain =
				A.QuestTypeTag ==
					TDTags::Quest_Type_Main.GetTag();

			const bool bBMain =
				B.QuestTypeTag ==
					TDTags::Quest_Type_Main.GetTag();

			if (bAMain != bBMain)
			{
				return bAMain;
			}

			return A.AcceptSequence
				< B.AcceptSequence;
		});

	return Result;
}

TArray<FTDQuestViewData>
UTDQuestComponent::GetQuestTrackerViews() const
{
	const TArray<FTDQuestViewData> AllViews =
		GetQuestViews();

	TArray<FTDQuestViewData> Result;
	int32 AddedSubQuestCount = 0;

	for (const FTDQuestViewData& View :
		AllViews)
	{
		const bool bMain =
			View.QuestTypeTag ==
				TDTags::Quest_Type_Main.GetTag();

		if (bMain)
		{
			if (!Result.ContainsByPredicate(
				[](const FTDQuestViewData& Existing)
				{
					return Existing.QuestTypeTag ==
						TDTags::Quest_Type_Main.GetTag();
				}))
			{
				Result.Add(View);
			}

			continue;
		}

		if (AddedSubQuestCount < 2)
		{
			Result.Add(View);
			++AddedSubQuestCount;
		}
	}

	return Result;
}

FName UTDQuestComponent::
FindBestTurnInQuestForTarget(
	ETDQuestTargetType TargetType,
	FName TargetId) const
{
	const FTDQuestRuntimeData* Best = nullptr;
	bool bBestIsMain = false;

	for (const FTDQuestRuntimeData& Entry :
		QuestEntries)
	{
		if (Entry.StateTag !=
			TDTags::Quest_State_ReadyToTurnIn.GetTag())
		{
			continue;
		}

		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition == nullptr
			|| Definition->TurnInTargetType != TargetType
			|| Definition->TurnInTargetId != TargetId)
		{
			continue;
		}

		const bool bCurrentIsMain =
			IsMainQuest(*Definition);

		if (Best == nullptr
			|| (bCurrentIsMain && !bBestIsMain)
			|| (bCurrentIsMain == bBestIsMain
				&& Entry.AcceptSequence
					< Best->AcceptSequence))
		{
			Best = &Entry;
			bBestIsMain = bCurrentIsMain;
		}
	}

	return Best ? Best->QuestId : NAME_None;
}

FName UTDQuestComponent::
FindBestOfferQuestForTarget(
	ETDQuestTargetType TargetType,
	FName TargetId,
	bool bIgnoreSubQuestLimitForMarker) const
{
	if (QuestTable == nullptr)
	{
		return NAME_None;
	}

	FName BestQuestId = NAME_None;
	bool bBestIsMain = false;

	QuestTable->ForeachRow<FTDQuestRow>(
		TEXT("FindBestOfferQuestForTarget"),
		[this,
		 TargetType,
		 TargetId,
		 bIgnoreSubQuestLimitForMarker,
		 &BestQuestId,
		 &bBestIsMain](
			const FName& RowName,
			const FTDQuestRow& Definition)
		{
			if (Definition.AcceptTargetType != TargetType
				|| Definition.AcceptTargetId != TargetId)
			{
				return;
			}

			const ETDQuestActionResult Result =
				CheckAcceptConditions(
					RowName,
					bIgnoreSubQuestLimitForMarker);

			if (Result !=
				ETDQuestActionResult::Success)
			{
				return;
			}

			const bool bCurrentIsMain =
				IsMainQuest(Definition);

			if (BestQuestId.IsNone()
				|| (bCurrentIsMain && !bBestIsMain))
			{
				BestQuestId = RowName;
				bBestIsMain = bCurrentIsMain;
			}
		});

	return BestQuestId;
}

FName UTDQuestComponent::FindActiveMainQuestForTarget(
	ETDQuestTargetType TargetType,
	FName TargetId) const
{
	if (TargetType == ETDQuestTargetType::None
		|| TargetId.IsNone())
	{
		return NAME_None;
	}

	const FTDQuestRuntimeData* Best = nullptr;

	for (const FTDQuestRuntimeData& Entry : QuestEntries)
	{
		// 아직 목표를 진행 중인 퀘스트만 검사한다.
		// 완료 보고 가능한 퀘스트는 별도 함수에서 처리한다.
		if (Entry.StateTag !=
			TDTags::Quest_State_Active.GetTag())
		{
			continue;
		}

		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition == nullptr
			|| !IsMainQuest(*Definition)
			|| Definition->bWaitForFutureContent
			|| Definition->Objectives.IsEmpty()
			|| Definition->InProgressDialogueRow.IsNone())
		{
			continue;
		}

		/*
		 * 현재 시스템의 대화형 메인:
		 * 모든 목표가 GameplayEvent인 메인 퀘스트.
		 *
		 * 몬스터 처치·아이템 수집 등의 퀘스트는
		 * 목표를 수행하는 동안 대화 우선권을 갖지 않는다.
		 */
		bool bDialogueOnlyQuest = true;

		for (const FTDQuestObjectiveDefinition& Objective :
			Definition->Objectives)
		{
			if (Objective.ObjectiveType !=
				ETDQuestObjectiveType::GameplayEvent)
			{
				bDialogueOnlyQuest = false;
				break;
			}
		}

		if (!bDialogueOnlyQuest)
		{
			continue;
		}

		/*
		 * 기존 데이터에서 목표 TargetId가 비어 있을 때
		 * 사용할 수락 대상·완료 대상 검사.
		 */
		const bool bMatchesAcceptTarget =
			Definition->AcceptTargetType == TargetType
			&& Definition->AcceptTargetId == TargetId;

		const bool bMatchesTurnInTarget =
			Definition->TurnInTargetType == TargetType
			&& Definition->TurnInTargetId == TargetId;

		bool bHasPendingDialogueAtTarget = false;

		for (int32 ObjectiveIndex = 0;
			ObjectiveIndex < Definition->Objectives.Num();
			++ObjectiveIndex)
		{
			const FTDQuestObjectiveDefinition& Objective =
				Definition->Objectives[ObjectiveIndex];

			const int32 CurrentCount =
				Entry.ObjectiveProgress.IsValidIndex(ObjectiveIndex)
					? Entry.ObjectiveProgress[ObjectiveIndex]
					: 0;

			const int32 RequiredCount =
				FMath::Max(1, Objective.RequiredCount);

			// 이미 끝난 대화 목표에는 다시 !를 표시하지 않는다.
			if (CurrentCount >= RequiredCount)
			{
				continue;
			}

			/*
			 * 목표에 TargetId가 지정되어 있으면 그 대상을 사용한다.
			 * 비어 있으면 기존 수락·완료 대상을 사용한다.
			 */
			const bool bMatchesObjectiveTarget =
				Objective.TargetId.IsNone()
					? (bMatchesAcceptTarget || bMatchesTurnInTarget)
					: Objective.TargetId == TargetId;

			if (bMatchesObjectiveTarget)
			{
				bHasPendingDialogueAtTarget = true;
				break;
			}
		}

		if (!bHasPendingDialogueAtTarget)
		{
			continue;
		}

		if (Best == nullptr
			|| Entry.AcceptSequence < Best->AcceptSequence)
		{
			Best = &Entry;
		}
	}

	return Best != nullptr
		? Best->QuestId
		: NAME_None;
}

FTDQuestMarkerView
UTDQuestComponent::GetQuestMarkerForTarget(
	ETDQuestTargetType TargetType,
	FName TargetId) const
{
	const auto MakeMarkerView =
		[this](
			ETDQuestMarkerType MarkerType,
			FName QuestId)
		{
			FTDQuestMarkerView Result;
			Result.MarkerType = MarkerType;
			Result.QuestId = QuestId;

			if (const FTDQuestRow* Definition =
				FindQuestDefinition(QuestId))
			{
				Result.QuestTypeTag =
					Definition->QuestTypeTag;
			}

			return Result;
		};

	/*
	 * 1. 완료 가능한 퀘스트 조회
	 */
	const FName TurnInQuestId =
		FindBestTurnInQuestForTarget(
			TargetType,
			TargetId);

	const FTDQuestRow* TurnInDefinition =
		TurnInQuestId.IsNone()
			? nullptr
			: FindQuestDefinition(
				TurnInQuestId);

	/*
	 * 1순위:
	 * 완료 가능한 메인 퀘스트
	 */
	if (TurnInDefinition != nullptr
		&& IsMainQuest(*TurnInDefinition))
	{
		return MakeMarkerView(
			ETDQuestMarkerType::TurnIn,
			TurnInQuestId);
	}

	/*
	 * 2순위:
	 * 현재 대상과 관련된 진행 중 메인 퀘스트
	 */
	const FName ActiveMainQuestId =
		FindActiveMainQuestForTarget(
			TargetType,
			TargetId);

	if (!ActiveMainQuestId.IsNone())
	{
		const FTDQuestRow* Definition =
			FindQuestDefinition(
				ActiveMainQuestId);

		if (Definition == nullptr)
		{
			return FTDQuestMarkerView();
		}

		bool bDialogueOnlyQuest =
			!Definition->Objectives.IsEmpty();

		for (const FTDQuestObjectiveDefinition&
			 Objective : Definition->Objectives)
		{
			if (Objective.ObjectiveType !=
				ETDQuestObjectiveType::
					GameplayEvent)
			{
				bDialogueOnlyQuest = false;
				break;
			}
		}

		if (bDialogueOnlyQuest
			&& !Definition
				->InProgressDialogueRow.IsNone())
		{
			return MakeMarkerView(
				ETDQuestMarkerType::Available,
				ActiveMainQuestId);
		}

		/*
		 * 몬스터 처치나 아이템 수집 중인
		 * 메인이 현재 NPC와 관련되어 있으면
		 * 서브 마커도 가린다.
		 */
		return FTDQuestMarkerView();
	}

	/*
	 * 3. 받을 수 있는 퀘스트 조회
	 */
	const FName OfferQuestId =
		FindBestOfferQuestForTarget(
			TargetType,
			TargetId,
			true);

	const FTDQuestRow* OfferDefinition =
		OfferQuestId.IsNone()
			? nullptr
			: FindQuestDefinition(
				OfferQuestId);

	/*
	 * 3순위:
	 * 받을 수 있는는 메인 퀘스트
	 */
	if (OfferDefinition != nullptr
		&& IsMainQuest(*OfferDefinition))
	{
		return MakeMarkerView(
			ETDQuestMarkerType::Available,
			OfferQuestId);
	}

	/*
	 * 4순위:
	 * 완료 가능한 서브/일일 퀘스트
	 */
	if (TurnInDefinition != nullptr)
	{
		return MakeMarkerView(
			ETDQuestMarkerType::TurnIn,
			TurnInQuestId);
	}

	/*
	 * 5순위:
	 * 진행 중인 서브/일일 퀘스트의
	 * GameplayEvent 목적지
	 *
	 * 여기서 점심 메뉴 퀘스트의
	 * 한수현 노란색 !가 결정된다.
	 */
	const FTDQuestRuntimeData*
		ActiveObjectiveQuest = nullptr;

	for (const FTDQuestRuntimeData& Entry :
		QuestEntries)
	{
		if (Entry.StateTag !=
			TDTags::Quest_State_Active.GetTag())
		{
			continue;
		}

		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition == nullptr
			|| IsMainQuest(*Definition))
		{
			continue;
		}

		bool bHasPendingObjectiveAtTarget =
			false;

		for (int32 ObjectiveIndex = 0;
			 ObjectiveIndex <
				Definition->Objectives.Num();
			 ++ObjectiveIndex)
		{
			const FTDQuestObjectiveDefinition&
				Objective =
					Definition
						->Objectives[
							ObjectiveIndex];

			if (Objective.ObjectiveType !=
					ETDQuestObjectiveType::
						GameplayEvent
				|| Objective.TargetId !=
					TargetId)
			{
				continue;
			}

			const int32 CurrentCount =
				Entry.ObjectiveProgress
					.IsValidIndex(
						ObjectiveIndex)
						? Entry
							.ObjectiveProgress[
								ObjectiveIndex]
						: 0;

			const int32 RequiredCount =
				FMath::Max(
					1,
					Objective.RequiredCount);

			if (CurrentCount < RequiredCount)
			{
				bHasPendingObjectiveAtTarget =
					true;
				break;
			}
		}

		if (!bHasPendingObjectiveAtTarget)
		{
			continue;
		}

		if (ActiveObjectiveQuest == nullptr
			|| Entry.AcceptSequence <
				ActiveObjectiveQuest
					->AcceptSequence)
		{
			ActiveObjectiveQuest = &Entry;
		}
	}

	if (ActiveObjectiveQuest != nullptr)
	{
		return MakeMarkerView(
			ETDQuestMarkerType::Available,
			ActiveObjectiveQuest->QuestId);
	}

	/*
	 * 6순위:
	 * 받을 수 있는 서브/일일 퀘스트
	 */
	if (OfferDefinition != nullptr)
	{
		return MakeMarkerView(
			ETDQuestMarkerType::Available,
			OfferQuestId);
	}

	return FTDQuestMarkerView();
}

void UTDQuestComponent::EnsureInitialMainQuest()
{
	for (const FTDQuestRuntimeData& Entry :
		QuestEntries)
	{
		if (!IsActiveState(Entry.StateTag))
		{
			continue;
		}

		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition != nullptr
			&& IsMainQuest(*Definition))
		{
			return;
		}
	}

	if (QuestTable == nullptr)
	{
		return;
	}

	FName InitialQuestId = NAME_None;

	QuestTable->ForeachRow<FTDQuestRow>(
		TEXT("EnsureInitialMainQuest"),
		[&InitialQuestId](
			const FName& RowName,
			const FTDQuestRow& Definition)
		{
			if (InitialQuestId.IsNone()
				&& Definition.bInitialMainQuest)
			{
				InitialQuestId = RowName;
			}
		});

	if (InitialQuestId.IsNone())
	{
		UE_LOG(LogTemp, Error,
			TEXT("DT_Quest에 bInitialMainQuest가 체크된 행이 없다."));
		return;
	}

	if (!HasQuest(InitialQuestId))
	{
		AcceptQuest(InitialQuestId);
	}
}

void UTDQuestComponent::
RepairDuplicateActiveMainQuests()
{
	TArray<int32> MainIndices;

	for (int32 Index = 0;
		Index < QuestEntries.Num();
		++Index)
	{
		if (!IsActiveState(
			QuestEntries[Index].StateTag))
		{
			continue;
		}

		const FTDQuestRow* Definition =
			FindQuestDefinition(
				QuestEntries[Index].QuestId);

		if (Definition != nullptr
			&& IsMainQuest(*Definition))
		{
			MainIndices.Add(Index);
		}
	}

	if (MainIndices.Num() <= 1)
	{
		return;
	}

	int32 BestIndex = MainIndices[0];
	double BestScore = -1.0;

	for (const int32 Index : MainIndices)
	{
		const FTDQuestRuntimeData& Entry =
			QuestEntries[Index];

		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition == nullptr)
		{
			continue;
		}

		double Score = 0.0;

		for (int32 ObjectiveIndex = 0;
			ObjectiveIndex <
				Definition->Objectives.Num();
			++ObjectiveIndex)
		{
			const int32 Required =
				FMath::Max(
					1,
					Definition->Objectives[
						ObjectiveIndex]
						.RequiredCount);

			const int32 Current =
				Entry.ObjectiveProgress.IsValidIndex(
					ObjectiveIndex)
					? Entry.ObjectiveProgress[
						ObjectiveIndex]
					: 0;

			Score += static_cast<double>(Current)
				/ Required;
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestIndex = Index;
		}
	}

	MainIndices.Sort(
		[](int32 A, int32 B)
		{
			return A > B;
		});

	for (const int32 Index : MainIndices)
	{
		if (Index == BestIndex)
		{
			continue;
		}

		if (const FTDQuestRow* Definition =
			FindQuestDefinition(
				QuestEntries[Index].QuestId))
		{
			RemoveQuestStateTags(*Definition);
		}

		UE_LOG(LogTemp, Error,
			TEXT("활성 메인 퀘스트 중복 복구: '%s' 제거"),
			*QuestEntries[Index].QuestId.ToString());

		QuestEntries.RemoveAt(Index);

		if (Index < BestIndex)
		{
			--BestIndex;
		}
	}
}

void UTDQuestComponent::ProcessAutomaticQuests()
{
	TArray<FName> AutomaticQuestIds;

	for (const FTDQuestRuntimeData& Entry :
		QuestEntries)
	{
		if (Entry.StateTag !=
			TDTags::Quest_State_ReadyToTurnIn.GetTag())
		{
			continue;
		}

		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition != nullptr
			&& Definition->bAutoCompleteWithoutTurnIn)
		{
			AutomaticQuestIds.Add(
				Entry.QuestId);
		}
	}

	for (const FName QuestId :
		AutomaticQuestIds)
	{
		TurnInQuest(QuestId);
	}
}

void UTDQuestComponent::HandleInventoryChanged()
{
	if (GetOwner() != nullptr
		&& GetOwner()->HasAuthority())
	{
		RecalculateInventoryObjectives();
	}
}

void UTDQuestComponent::HandleZoneChanged(
	FGameplayTag NewZoneId)
{
	if (GetOwner() != nullptr
		&& GetOwner()->HasAuthority())
	{
		ReportZoneEntered(NewZoneId);
	}
}

void UTDQuestComponent::HandleCharacterSelected()
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority())
	{
		return;
	}

	RepairDuplicateActiveMainQuests();
	EnsureInitialMainQuest();
	RecalculateInventoryObjectives();

	const ATDPlayerState* PlayerState =
		Cast<ATDPlayerState>(OwnerActor);

	if (PlayerState != nullptr
		&& PlayerState->GetCurrentZoneId().IsValid())
	{
		ReportZoneEntered(
			PlayerState->GetCurrentZoneId());
	}

	ProcessAutomaticQuests();
}

void UTDQuestComponent::NotifyQuestListChanged()
{
	OnQuestListChanged.Broadcast();

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void UTDQuestComponent::OnRep_QuestEntries()
{
	OnQuestListChanged.Broadcast();
}

void UTDQuestComponent::OnRep_AffectionEntries()
{
	for (const FTDAffectionRuntimeData& Entry :
		AffectionEntries)
	{
		OnAffectionChanged.Broadcast(
			Entry.NPCId,
			Entry.Points);
	}
}

void UTDQuestComponent::WriteSaveData(
	FTDPlayerSaveData& Out) const
{
	Out.QuestStates = QuestEntries;
	Out.AffectionStates = AffectionEntries;
}

void UTDQuestComponent::ReadSaveData(
	const FTDPlayerSaveData& In)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority())
	{
		return;
	}

	QuestEntries.Reset();
	AffectionEntries.Reset();
	NextAcceptSequence = 1;

	if (QuestTable == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Quest ReadSaveData: QuestTable이 없다."));
		return;
	}

	TSet<FName> LoadedQuestIds;

	for (const FTDQuestRuntimeData& Saved :
		In.QuestStates)
	{
		const FTDQuestRow* Definition =
			FindQuestDefinition(Saved.QuestId);

		if (Definition == nullptr
			|| LoadedQuestIds.Contains(
				Saved.QuestId))
		{
			continue;
		}

		FTDQuestRuntimeData Sanitized;
		Sanitized.QuestId = Saved.QuestId;
		Sanitized.StateTag = Saved.StateTag;
		Sanitized.AcceptSequence =
			FMath::Max<int64>(
				1,
				Saved.AcceptSequence);
		Sanitized.CompletedKstDayKey =
			FMath::Max(
				0,
				Saved.CompletedKstDayKey);

		Sanitized.ObjectiveProgress.Init(
			0,
			Definition->Objectives.Num());

		for (int32 Index = 0;
			Index < Definition->Objectives.Num();
			++Index)
		{
			const int32 Required =
				FMath::Max(
					1,
					Definition->Objectives[Index]
						.RequiredCount);

			const int32 SavedProgress =
				Saved.ObjectiveProgress.IsValidIndex(
					Index)
					? Saved.ObjectiveProgress[Index]
					: 0;

			Sanitized.ObjectiveProgress[Index] =
				FMath::Clamp(
					SavedProgress,
					0,
					Required);
		}

		const bool bCompleted =
			Sanitized.StateTag ==
				TDTags::Quest_State_Completed.GetTag();

		if (!bCompleted)
		{
			Sanitized.StateTag =
				TDTags::Quest_State_Active.GetTag();

			RefreshQuestState(
				Sanitized,
				*Definition);
		}

		NextAcceptSequence =
			FMath::Max(
				NextAcceptSequence,
				Sanitized.AcceptSequence + 1);

		LoadedQuestIds.Add(
			Sanitized.QuestId);

		QuestEntries.Add(
			MoveTemp(Sanitized));
	}

	TSet<FName> LoadedNPCIds;

	for (const FTDAffectionRuntimeData& Saved :
		In.AffectionStates)
	{
		if (Saved.NPCId.IsNone()
			|| LoadedNPCIds.Contains(Saved.NPCId))
		{
			continue;
		}

		FTDAffectionRuntimeData Sanitized;
		Sanitized.NPCId = Saved.NPCId;
		Sanitized.Points =
			FMath::Max(0, Saved.Points);
		Sanitized.LastGiftKstDayKey =
			FMath::Max(
				0,
				Saved.LastGiftKstDayKey);

		LoadedNPCIds.Add(Sanitized.NPCId);
		AffectionEntries.Add(Sanitized);
	}

	RepairDuplicateActiveMainQuests();

	for (const FTDQuestRuntimeData& Entry :
		QuestEntries)
	{
		if (const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId))
		{
			ApplyQuestStateTags(
				Entry,
				*Definition);
		}
	}

	const ATDPlayerState* PlayerState =
		Cast<ATDPlayerState>(OwnerActor);

	if (PlayerState != nullptr
		&& PlayerState->HasSelectedCharacter())
	{
		EnsureInitialMainQuest();
		RecalculateInventoryObjectives();

		if (PlayerState
			->GetCurrentZoneId()
			.IsValid())
		{
			ReportZoneEntered(
				PlayerState
					->GetCurrentZoneId());
		}
	}

	ProcessAutomaticQuests();
	NotifyQuestListChanged();
}