#include "Quest/TDPersonalWorldStateComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UTDPersonalWorldStateComponent::UTDPersonalWorldStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTDPersonalWorldStateComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 자기 퀘스트와 획득한 상자만 알면 된다.
	DOREPLIFETIME_CONDITION(
		UTDPersonalWorldStateComponent,
		QuestProgressTags,
		COND_OwnerOnly);

	DOREPLIFETIME_CONDITION(
		UTDPersonalWorldStateComponent,
		ClaimedChestIds,
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
	return Condition.IsSatisfiedBy(QuestProgressTags);
}

bool UTDPersonalWorldStateComponent::HasClaimedChest(
	FName ChestId) const
{
	return !ChestId.IsNone()
		&& ClaimedChestIds.Contains(ChestId);
}

bool UTDPersonalWorldStateComponent::AddQuestTag(
	FGameplayTag Tag)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AddQuestTag는 서버에서만 호출할 수 있다."));
		return false;
	}

	if (!Tag.IsValid()
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

	if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("RemoveQuestTag는 서버에서만 호출할 수 있다."));
		return false;
	}

	if (!Tag.IsValid()
		|| !QuestProgressTags.HasTagExact(Tag))
	{
		return false;
	}

	QuestProgressTags.RemoveTag(Tag);
	NotifyStateChanged();

	return true;
}

bool UTDPersonalWorldStateComponent::MarkChestClaimed(
	FName ChestId)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("MarkChestClaimed는 서버에서만 호출할 수 있다."));
		return false;
	}

	if (ChestId.IsNone()
		|| ClaimedChestIds.Contains(ChestId))
	{
		return false;
	}

	ClaimedChestIds.Add(ChestId);
	NotifyStateChanged();

	return true;
}

void UTDPersonalWorldStateComponent::WriteSaveData(
	FTDPlayerSaveData& Out) const
{
	Out.QuestProgressTags = QuestProgressTags;
	Out.ClaimedChestIds = ClaimedChestIds;
}

void UTDPersonalWorldStateComponent::ReadSaveData(
	const FTDPlayerSaveData& In)
{
	AActor* OwnerActor = GetOwner();

	if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PersonalWorldState ReadSaveData는 서버에서만 호출해야 한다."));
		return;
	}

	QuestProgressTags = In.QuestProgressTags;

	// 저장 데이터에 잘못된 None 또는 중복 ID가 있어도 정리해서 읽는다.
	ClaimedChestIds.Reset();

	for (const FName ChestId : In.ClaimedChestIds)
	{
		if (!ChestId.IsNone())
		{
			ClaimedChestIds.AddUnique(ChestId);
		}
	}

	NotifyStateChanged();
}

void UTDPersonalWorldStateComponent::NotifyStateChanged()
{
	OnPersonalWorldStateChanged.Broadcast();

	if (AActor* OwnerActor = GetOwner())
	{
		// PlayerState의 기본 전송 주기를 기다리지 않고 가능한 한 빨리 전송한다.
		OwnerActor->ForceNetUpdate();
	}
}

void UTDPersonalWorldStateComponent::OnRep_QuestProgressTags()
{
	OnPersonalWorldStateChanged.Broadcast();
}

void UTDPersonalWorldStateComponent::OnRep_ClaimedChestIds()
{
	OnPersonalWorldStateChanged.Broadcast();
}