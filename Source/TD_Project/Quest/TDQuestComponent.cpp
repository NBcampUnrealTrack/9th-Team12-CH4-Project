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

bool UTDQuestComponent::HasQuest(FName QuestId) const
{
	return FindQuest(QuestId) != nullptr;
}

FGameplayTag UTDQuestComponent::GetQuestStateTag(
	FName QuestId) const
{
	const FTDQuestRuntimeData* Entry = FindQuest(QuestId);
	return Entry ? Entry->StateTag : FGameplayTag();
}

bool UTDQuestComponent::IsReadyToTurnIn(
	const FTDQuestRuntimeData& Entry,
	const FTDQuestRow& Definition) const
{
	if (Entry.ObjectiveProgress.Num() != Definition.Objectives.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < Definition.Objectives.Num(); ++Index)
	{
		const int32 Required =
			FMath::Max(1, Definition.Objectives[Index].RequiredCount);

		if (Entry.ObjectiveProgress[Index] < Required)
		{
			return false;
		}
	}

	return true;
}

void UTDQuestComponent::ApplyQuestStateTags(
	const FTDQuestRuntimeData& Entry,
	const FTDQuestRow& Definition)
{
	ATDPlayerState* PlayerState =
		Cast<ATDPlayerState>(GetOwner());

	UTDPersonalWorldStateComponent* PersonalState =
		PlayerState
			? PlayerState->GetPersonalWorldStateComponent()
			: nullptr;

	if (PersonalState == nullptr)
	{
		return;
	}

	// 이전 단계 태그를 모두 지우고 현재 단계 하나만 넣는다.
	PersonalState->RemoveQuestTag(Definition.AcceptedTag);
	PersonalState->RemoveQuestTag(Definition.ReadyTag);
	PersonalState->RemoveQuestTag(Definition.CompletedTag);

	if (Entry.StateTag == TDTags::Quest_State_Active.GetTag())
	{
		PersonalState->AddQuestTag(Definition.AcceptedTag);
	}
	else if (Entry.StateTag ==
		TDTags::Quest_State_ReadyToTurnIn.GetTag())
	{
		PersonalState->AddQuestTag(Definition.ReadyTag);
	}
	else if (Entry.StateTag ==
		TDTags::Quest_State_Completed.GetTag())
	{
		PersonalState->AddQuestTag(Definition.CompletedTag);
	}
}

ETDQuestActionResult UTDQuestComponent::AcceptQuest(
	FName QuestId)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
	{
		return ETDQuestActionResult::InvalidDefinition;
	}

	const FTDQuestRow* Definition =
		FindQuestDefinition(QuestId);

	if (Definition == nullptr)
	{
		return ETDQuestActionResult::InvalidDefinition;
	}

	if (const FTDQuestRuntimeData* Existing = FindQuest(QuestId))
	{
		return Existing->StateTag ==
			TDTags::Quest_State_Completed.GetTag()
				? ETDQuestActionResult::AlreadyCompleted
				: ETDQuestActionResult::AlreadyAccepted;
	}

	ATDPlayerState* PlayerState =
		Cast<ATDPlayerState>(OwnerActor);

	UTDPersonalWorldStateComponent* PersonalState =
		PlayerState
			? PlayerState->GetPersonalWorldStateComponent()
			: nullptr;

	if (PersonalState == nullptr)
	{
		return ETDQuestActionResult::InvalidDefinition;
	}

	if (!PersonalState->MatchesCondition(
		Definition->AcceptCondition))
	{
		return ETDQuestActionResult::PrerequisiteNotMet;
	}

	FTDQuestRuntimeData NewEntry;
	NewEntry.QuestId = QuestId;
	NewEntry.StateTag =
		TDTags::Quest_State_Active.GetTag();

	NewEntry.ObjectiveProgress.Init(
		0,
		Definition->Objectives.Num());

	// 목표가 없는 퀘스트는 수락 즉시 완료 보고 가능 상태가 된다.
	if (IsReadyToTurnIn(NewEntry, *Definition))
	{
		NewEntry.StateTag =
			TDTags::Quest_State_ReadyToTurnIn.GetTag();
	}

	QuestEntries.Add(MoveTemp(NewEntry));

	ApplyQuestStateTags(
		QuestEntries.Last(),
		*Definition);

	NotifyQuestListChanged();

	UE_LOG(LogTemp, Log,
		TEXT("퀘스트 수락: Player='%s', Quest='%s'"),
		*GetNameSafe(OwnerActor),
		*QuestId.ToString());

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

	for (FTDQuestRuntimeData& Entry : QuestEntries)
	{
		if (Entry.StateTag !=
			TDTags::Quest_State_Active.GetTag())
		{
			continue;
		}

		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition == nullptr)
		{
			continue;
		}

		if (Entry.ObjectiveProgress.Num()
			!= Definition->Objectives.Num())
		{
			Entry.ObjectiveProgress.SetNumZeroed(
				Definition->Objectives.Num());
		}

		bool bChanged = false;

		for (int32 Index = 0;
			Index < Definition->Objectives.Num();
			++Index)
		{
			const FTDQuestObjectiveDefinition& Objective =
				Definition->Objectives[Index];

			if (!Objective.EventTag.IsValid()
				|| !EventTag.MatchesTag(Objective.EventTag))
			{
				continue;
			}

			const int32 Required =
				FMath::Max(1, Objective.RequiredCount);

			const int32 Previous =
				Entry.ObjectiveProgress[Index];

			Entry.ObjectiveProgress[Index] =
				FMath::Clamp(
					Previous + Amount,
					0,
					Required);

			bChanged |=
				Entry.ObjectiveProgress[Index] != Previous;
		}

		if (!bChanged)
		{
			continue;
		}

		++ChangedQuestCount;

		if (IsReadyToTurnIn(Entry, *Definition))
		{
			Entry.StateTag =
				TDTags::Quest_State_ReadyToTurnIn.GetTag();

			ApplyQuestStateTags(
				Entry,
				*Definition);
		}
	}

	if (ChangedQuestCount > 0)
	{
		NotifyQuestListChanged();
	}

	return ChangedQuestCount;
}

bool UTDQuestComponent::CanReceiveAllRewards(
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

	TMap<FName, int32> AggregatedRewards;

	for (const FTDQuestItemReward& Reward :
		Definition.ItemRewards)
	{
		if (Reward.Count <= 0)
		{
			continue;
		}

		if (Reward.ItemId.IsNone())
		{
			OutFailure =
				ETDQuestActionResult::InvalidDefinition;
			return false;
		}

		AggregatedRewards.FindOrAdd(Reward.ItemId)
			+= Reward.Count;
	}

	if (AggregatedRewards.IsEmpty())
	{
		return true;
	}

	if (Inventory == nullptr)
	{
		OutFailure =
			ETDQuestActionResult::InvalidDefinition;
		return false;
	}

	int32 RemainingFreeSlots =
		Inventory->GetSlotCapacity()
		- Inventory->GetUsedSlotCount();

	for (const TPair<FName, int32>& Pair :
		AggregatedRewards)
	{
		const FTDItemRow* ItemDefinition =
			Inventory->FindItemDefinition(Pair.Key);

		if (ItemDefinition == nullptr)
		{
			OutFailure =
				ETDQuestActionResult::InvalidDefinition;
			return false;
		}

		const int32 MaxStack =
			ItemDefinition->bStackable
				? FMath::Max(1, ItemDefinition->MaxStackSize)
				: 1;

		int32 RemainingCount = Pair.Value;

		if (ItemDefinition->bStackable)
		{
			for (const FTDItemInstance& Existing :
				Inventory->GetItems())
			{
				if (Existing.ItemId != Pair.Key
					|| Existing.Count >= MaxStack)
				{
					continue;
				}

				RemainingCount -=
					MaxStack - Existing.Count;

				if (RemainingCount <= 0)
				{
					break;
				}
			}
		}

		if (RemainingCount > 0)
		{
			RemainingFreeSlots -=
				FMath::DivideAndRoundUp(
					RemainingCount,
					MaxStack);
		}

		if (RemainingFreeSlots < 0)
		{
			OutFailure =
				ETDQuestActionResult::InventoryFull;
			return false;
		}
	}

	return true;
}

ETDQuestActionResult UTDQuestComponent::TurnInQuest(
	FName QuestId)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
	{
		return ETDQuestActionResult::InvalidDefinition;
	}

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

	if (Entry->StateTag !=
		TDTags::Quest_State_ReadyToTurnIn.GetTag())
	{
		return ETDQuestActionResult::NotReady;
	}

	const FTDQuestRow* Definition =
		FindQuestDefinition(QuestId);

	if (Definition == nullptr)
	{
		return ETDQuestActionResult::InvalidDefinition;
	}

	ETDQuestActionResult RewardFailure;

	if (!CanReceiveAllRewards(
		*Definition,
		RewardFailure))
	{
		return RewardFailure;
	}

	ATDPlayerState* PlayerState =
		Cast<ATDPlayerState>(OwnerActor);

	UTDInventoryComponent* Inventory =
		PlayerState
			? PlayerState->GetInventoryComponent()
			: nullptr;

	for (const FTDQuestItemReward& Reward :
		Definition->ItemRewards)
	{
		if (Reward.Count <= 0)
		{
			continue;
		}

		if (Inventory == nullptr
			|| !Inventory->AddItem(
				Reward.ItemId,
				Reward.Count))
		{
			// 사전 계산을 통과했다면 정상적으로는 도달하지 않는다.
			UE_LOG(LogTemp, Error,
				TEXT("퀘스트 보상 사전 검사 후 지급 실패: Quest='%s', Item='%s'"),
				*QuestId.ToString(),
				*Reward.ItemId.ToString());

			return ETDQuestActionResult::InvalidDefinition;
		}
	}

	if (Definition->ExpReward > 0)
	{
		UTDProgressionComponent* Progression =
			PlayerState
				? PlayerState->GetProgressionComponent()
				: nullptr;

		if (Progression == nullptr)
		{
			return ETDQuestActionResult::InvalidDefinition;
		}

		Progression->AddExp(
			Definition->ExpReward);
	}

	Entry->StateTag =
		TDTags::Quest_State_Completed.GetTag();

	ApplyQuestStateTags(
		*Entry,
		*Definition);

	NotifyQuestListChanged();

	UE_LOG(LogTemp, Log,
		TEXT("퀘스트 완료: Player='%s', Quest='%s', Exp=%d"),
		*GetNameSafe(OwnerActor),
		*QuestId.ToString(),
		Definition->ExpReward);

	return ETDQuestActionResult::Success;
}

TArray<FTDQuestViewData>
UTDQuestComponent::GetQuestViews() const
{
	TArray<FTDQuestViewData> Result;
	Result.Reserve(QuestEntries.Num());

	for (const FTDQuestRuntimeData& Entry :
		QuestEntries)
	{
		const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId);

		if (Definition == nullptr)
		{
			continue;
		}

		FTDQuestViewData& View =
			Result.AddDefaulted_GetRef();

		View.QuestId = Entry.QuestId;
		View.QuestTypeTag =
			Definition->QuestTypeTag;
		View.StateTag = Entry.StateTag;
		View.DisplayName =
			Definition->DisplayName;
		View.Description =
			Definition->Description;

		for (int32 Index = 0;
			Index < Definition->Objectives.Num();
			++Index)
		{
			const FTDQuestObjectiveDefinition& Objective =
				Definition->Objectives[Index];

			FTDQuestObjectiveView& ObjectiveView =
				View.Objectives.AddDefaulted_GetRef();

			ObjectiveView.Description =
				Objective.Description;

			ObjectiveView.RequiredCount =
				FMath::Max(1, Objective.RequiredCount);

			ObjectiveView.CurrentCount =
				Entry.ObjectiveProgress.IsValidIndex(Index)
					? Entry.ObjectiveProgress[Index]
					: 0;
		}
	}

	return Result;
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

void UTDQuestComponent::WriteSaveData(
	FTDPlayerSaveData& Out) const
{
	Out.QuestStates = QuestEntries;
}

void UTDQuestComponent::ReadSaveData(
	const FTDPlayerSaveData& In)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
	{
		return;
	}

	QuestEntries.Reset();

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
			|| LoadedQuestIds.Contains(Saved.QuestId))
		{
			continue;
		}

		FTDQuestRuntimeData Sanitized;
		Sanitized.QuestId = Saved.QuestId;
		Sanitized.StateTag = Saved.StateTag;
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
					Definition->Objectives[Index].RequiredCount);

			const int32 SavedProgress =
				Saved.ObjectiveProgress.IsValidIndex(Index)
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
				IsReadyToTurnIn(Sanitized, *Definition)
					? TDTags::Quest_State_ReadyToTurnIn.GetTag()
					: TDTags::Quest_State_Active.GetTag();
		}

		LoadedQuestIds.Add(Sanitized.QuestId);
		QuestEntries.Add(MoveTemp(Sanitized));
	}

	for (const FTDQuestRuntimeData& Entry : QuestEntries)
	{
		if (const FTDQuestRow* Definition =
			FindQuestDefinition(Entry.QuestId))
		{
			ApplyQuestStateTags(
				Entry,
				*Definition);
		}
	}

	NotifyQuestListChanged();
}