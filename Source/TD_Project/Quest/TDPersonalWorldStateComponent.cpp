#include "Quest/TDPersonalWorldStateComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "World/TDKoreanDailyResetSubsystem.h"

UTDPersonalWorldStateComponent::
UTDPersonalWorldStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTDPersonalWorldStateComponent::
GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>&
		OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(
		OutLifetimeProps);

	DOREPLIFETIME_CONDITION(
		UTDPersonalWorldStateComponent,
		QuestProgressTags,
		COND_OwnerOnly);

	DOREPLIFETIME_CONDITION(
		UTDPersonalWorldStateComponent,
		ClaimedChestIds,
		COND_OwnerOnly);

	DOREPLIFETIME_CONDITION(
		UTDPersonalWorldStateComponent,
		ChestClaimRecords,
		COND_OwnerOnly);

	DOREPLIFETIME_CONDITION(
		UTDPersonalWorldStateComponent,
		SeenChapterIds,
		COND_OwnerOnly);

	DOREPLIFETIME_CONDITION(
		UTDPersonalWorldStateComponent,
		NpcGiftRecords,
		COND_OwnerOnly);
}

bool UTDPersonalWorldStateComponent::HasQuestTag(
	FGameplayTag Tag,
	bool bExactMatch) const
{
	if (!Tag.IsValid())
	{
		return false;
	}

	return bExactMatch
		? QuestProgressTags.HasTagExact(Tag)
		: QuestProgressTags.HasTag(Tag);
}

bool UTDPersonalWorldStateComponent::MatchesCondition(
	const FTDWorldCondition& Condition) const
{
	return Condition.IsSatisfiedBy(
		QuestProgressTags);
}

bool UTDPersonalWorldStateComponent::AddQuestTag(
	FGameplayTag Tag)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority()
		|| !Tag.IsValid()
		|| QuestProgressTags.HasTagExact(Tag))
	{
		return false;
	}

	QuestProgressTags.AddTag(Tag);
	NotifyStateChanged();
	return true;
}

bool UTDPersonalWorldStateComponent::RemoveQuestTag(
	FGameplayTag Tag)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority()
		|| !Tag.IsValid()
		|| !QuestProgressTags.HasTagExact(Tag))
	{
		return false;
	}

	QuestProgressTags.RemoveTag(Tag);
	NotifyStateChanged();
	return true;
}

const FTDChestClaimRecord*
UTDPersonalWorldStateComponent::FindChestClaim(
	FName ChestId) const
{
	return ChestClaimRecords.FindByPredicate(
		[ChestId](const FTDChestClaimRecord& Record)
		{
			return Record.ChestId == ChestId;
		});
}

FTDChestClaimRecord*
UTDPersonalWorldStateComponent::FindMutableChestClaim(
	FName ChestId)
{
	return ChestClaimRecords.FindByPredicate(
		[ChestId](const FTDChestClaimRecord& Record)
		{
			return Record.ChestId == ChestId;
		});
}

bool UTDPersonalWorldStateComponent::HasClaimedChest(
	FName ChestId) const
{
	return !ChestId.IsNone()
		&& (ClaimedChestIds.Contains(ChestId)
			|| FindChestClaim(ChestId) != nullptr);
}

bool UTDPersonalWorldStateComponent::CanClaimChest(
	FName ChestId,
	ETDChestResetType ResetType) const
{
	if (ChestId.IsNone())
	{
		return false;
	}

	const FTDChestClaimRecord* Record =
		FindChestClaim(ChestId);

	if (ResetType ==
		ETDChestResetType::OneTime)
	{
		return Record == nullptr
			&& !ClaimedChestIds.Contains(ChestId);
	}

	if (Record == nullptr)
	{
		return true;
	}

	return Record->LastClaimKstDayKey
		!= UTDKoreanDailyResetSubsystem::
			GetCurrentKstDayKey();
}

bool UTDPersonalWorldStateComponent::MarkChestClaimed(
	FName ChestId)
{
	return MarkChestClaimedWithReset(
		ChestId,
		ETDChestResetType::OneTime);
}

bool UTDPersonalWorldStateComponent::
MarkChestClaimedWithReset(
	FName ChestId,
	ETDChestResetType ResetType)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority()
		|| !CanClaimChest(ChestId, ResetType))
	{
		return false;
	}

	FTDChestClaimRecord* Record =
		FindMutableChestClaim(ChestId);

	if (Record == nullptr)
	{
		FTDChestClaimRecord& Added =
			ChestClaimRecords.AddDefaulted_GetRef();

		Added.ChestId = ChestId;
		Record = &Added;
	}

	if (ResetType ==
		ETDChestResetType::OneTime)
	{
		Record->LastClaimKstDayKey = 0;
		ClaimedChestIds.AddUnique(ChestId);
	}
	else
	{
		Record->LastClaimKstDayKey =
			UTDKoreanDailyResetSubsystem::
				GetCurrentKstDayKey();
	}

	NotifyStateChanged();
	return true;
}

bool UTDPersonalWorldStateComponent::HasSeenChapter(
	FName ChapterId) const
{
	return !ChapterId.IsNone()
		&& SeenChapterIds.Contains(ChapterId);
}

bool UTDPersonalWorldStateComponent::MarkChapterSeen(
	FName ChapterId)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority()
		|| ChapterId.IsNone()
		|| SeenChapterIds.Contains(ChapterId))
	{
		return false;
	}

	SeenChapterIds.Add(ChapterId);
	NotifyStateChanged();
	return true;
}

const FTDNpcGiftRecord*
UTDPersonalWorldStateComponent::FindGiftRecord(
	FName NPCId) const
{
	return NpcGiftRecords.FindByPredicate(
		[NPCId](const FTDNpcGiftRecord& Record)
		{
			return Record.NPCId == NPCId;
		});
}

FTDNpcGiftRecord*
UTDPersonalWorldStateComponent::FindMutableGiftRecord(
	FName NPCId)
{
	return NpcGiftRecords.FindByPredicate(
		[NPCId](const FTDNpcGiftRecord& Record)
		{
			return Record.NPCId == NPCId;
		});
}

bool UTDPersonalWorldStateComponent::CanGiftToNPC(
	FName NPCId) const
{
	if (NPCId.IsNone())
	{
		return false;
	}

	const FTDNpcGiftRecord* Record =
		FindGiftRecord(NPCId);

	return Record == nullptr
		|| Record->LastGiftKstDayKey
			!= UTDKoreanDailyResetSubsystem::
				GetCurrentKstDayKey();
}

bool UTDPersonalWorldStateComponent::MarkGiftGiven(
	FName NPCId)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority()
		|| !CanGiftToNPC(NPCId))
	{
		return false;
	}

	FTDNpcGiftRecord* Record =
		FindMutableGiftRecord(NPCId);

	if (Record == nullptr)
	{
		FTDNpcGiftRecord& Added =
			NpcGiftRecords.AddDefaulted_GetRef();

		Added.NPCId = NPCId;
		Record = &Added;
	}

	Record->LastGiftKstDayKey =
		UTDKoreanDailyResetSubsystem::
			GetCurrentKstDayKey();

	NotifyStateChanged();
	return true;
}

void UTDPersonalWorldStateComponent::
WriteSaveData(FTDPlayerSaveData& Out) const
{
	Out.QuestProgressTags = QuestProgressTags;
	Out.ClaimedChestIds = ClaimedChestIds;
	Out.ChestClaimRecords = ChestClaimRecords;
	Out.SeenChapterIds = SeenChapterIds;
	Out.NpcGiftRecords = NpcGiftRecords;
}

void UTDPersonalWorldStateComponent::
ReadSaveData(const FTDPlayerSaveData& In)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr
		|| !OwnerActor->HasAuthority())
	{
		return;
	}

	QuestProgressTags = In.QuestProgressTags;

	ClaimedChestIds.Reset();
	ChestClaimRecords.Reset();
	SeenChapterIds.Reset();
	NpcGiftRecords.Reset();

	for (const FName ChestId :
		In.ClaimedChestIds)
	{
		if (!ChestId.IsNone())
		{
			ClaimedChestIds.AddUnique(ChestId);
		}
	}

	for (const FTDChestClaimRecord& Saved :
		In.ChestClaimRecords)
	{
		if (Saved.ChestId.IsNone()
			|| FindChestClaim(Saved.ChestId)
				!= nullptr)
		{
			continue;
		}

		ChestClaimRecords.Add(Saved);
	}

	/**
	 * 구버전 ClaimedChestIds를 영구 상자 기록으로 변환한다.
	 */
	for (const FName LegacyChestId :
		ClaimedChestIds)
	{
		if (FindChestClaim(LegacyChestId)
			== nullptr)
		{
			FTDChestClaimRecord& Added =
				ChestClaimRecords
					.AddDefaulted_GetRef();

			Added.ChestId = LegacyChestId;
			Added.LastClaimKstDayKey = 0;
		}
	}

	for (const FName ChapterId :
		In.SeenChapterIds)
	{
		if (!ChapterId.IsNone())
		{
			SeenChapterIds.AddUnique(ChapterId);
		}
	}

	for (const FTDNpcGiftRecord& Saved :
		In.NpcGiftRecords)
	{
		if (Saved.NPCId.IsNone()
			|| FindGiftRecord(Saved.NPCId)
				!= nullptr)
		{
			continue;
		}

		NpcGiftRecords.Add(Saved);
	}

	NotifyStateChanged();
}

void UTDPersonalWorldStateComponent::
NotifyStateChanged()
{
	OnPersonalWorldStateChanged.Broadcast();

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void UTDPersonalWorldStateComponent::OnRep_State()
{
	OnPersonalWorldStateChanged.Broadcast();
}