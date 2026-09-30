#include "World/TDTreasureChest.h"

#include "Character/TDPlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Data/TDItemRow.h"
#include "Data/TDTreasureChestRow.h"
#include "Items/TDInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PaperFlipbookComponent.h"
#include "Player/TDPlayerController.h"
#include "Player/TDPlayerState.h"
#include "Quest/TDPersonalWorldStateComponent.h"
#include "Quest/TDQuestComponent.h"
#include "Stats/TDProgressionComponent.h"
#include "World/TDKoreanDailyResetSubsystem.h"

ATDTreasureChest::ATDTreasureChest()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot =
		CreateDefaultSubobject<USceneComponent>(
			TEXT("SceneRoot"));

	SetRootComponent(SceneRoot);

	SpriteComponent =
		CreateDefaultSubobject<UPaperFlipbookComponent>(
			TEXT("SpriteComponent"));

	SpriteComponent->SetupAttachment(SceneRoot);
	SpriteComponent->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	SpriteComponent->SetUsingAbsoluteRotation(true);

	InteractionSphere =
		CreateDefaultSubobject<USphereComponent>(
			TEXT("InteractionSphere"));

	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->SetSphereRadius(150.0f);
	InteractionSphere->SetCollisionObjectType(
		ECC_WorldDynamic);
	InteractionSphere->SetCollisionEnabled(
		ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(
		ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(
		ECC_Pawn,
		ECR_Overlap);
}

void ATDTreasureChest::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	TryBindToLocalState();

	if (LocalPersonalState == nullptr)
	{
		GetWorldTimerManager().SetTimer(
			BindRetryTimerHandle,
			this,
			&ATDTreasureChest::
				TryBindToLocalState,
			0.25f,
			true);
	}
}

void ATDTreasureChest::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(
		BindRetryTimerHandle);

	GetWorldTimerManager().ClearTimer(
		LocalHideTimerHandle);

	if (LocalPersonalState != nullptr)
	{
		LocalPersonalState
			->OnPersonalWorldStateChanged
			.RemoveDynamic(
				this,
				&ATDTreasureChest::HandlePersonalStateChanged);
	}

	if (DailyResetSubsystem != nullptr)
	{
		DailyResetSubsystem
			->OnKoreanDayChanged
			.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

const FTDTreasureChestRow*
ATDTreasureChest::GetDefinitionRow() const
{
	if (ChestDefinition.DataTable == nullptr
		|| ChestDefinition.RowName.IsNone())
	{
		return nullptr;
	}

	return ChestDefinition.GetRow<
		FTDTreasureChestRow>(
			TEXT("ATDTreasureChest"));
}

bool ATDTreasureChest::CanInteract_Implementation(
	ATDPlayerCharacter* Player) const
{
	if (!IsValid(Player))
	{
		return false;
	}

	const FTDTreasureChestRow* Row =
		GetDefinitionRow();

	const ATDPlayerState* PlayerState =
		Player->GetPlayerState<ATDPlayerState>();

	const UTDPersonalWorldStateComponent* Personal =
		PlayerState
			? PlayerState
				->GetPersonalWorldStateComponent()
			: nullptr;

	if (Row == nullptr
		|| Personal == nullptr
		|| !Personal->MatchesCondition(
			Row->Condition))
	{
		return false;
	}

	return Personal->CanClaimChest(
		GetChestId(),
		Row->ResetType);
}

void ATDTreasureChest::RollRandomRewards(
	const FTDTreasureChestRow& Definition,
	TArray<FTDChestItemReward>& OutRewards) const
{
	if (!Definition.bUseRandomRewards)
	{
		return;
	}

	const int32 SafeMin =
		FMath::Clamp(
			Definition.MinRollCount,
			0,
			2);

	const int32 SafeMax =
		FMath::Clamp(
			FMath::Max(
				SafeMin,
				Definition.MaxRollCount),
			0,
			2);

	const int32 RollCount =
		FMath::RandRange(
			SafeMin,
			SafeMax);

	float TotalWeight =
		FMath::Max(
			0.0f,
			Definition.MissWeight);

	for (const FTDChestRandomReward& Reward :
		Definition.RandomRewards)
	{
		if (!Reward.ItemId.IsNone())
		{
			TotalWeight +=
				FMath::Max(
					0.0f,
					Reward.Weight);
		}
	}

	for (int32 RollIndex = 0;
		RollIndex < RollCount;
		++RollIndex)
	{
		if (TotalWeight <= 0.0f)
		{
			continue;
		}

		float Pick =
			FMath::FRandRange(
				0.0f,
				TotalWeight);

		const float MissWeight =
			FMath::Max(
				0.0f,
				Definition.MissWeight);

		if (Pick < MissWeight)
		{
			continue;
		}

		Pick -= MissWeight;

		for (const FTDChestRandomReward& Reward :
			Definition.RandomRewards)
		{
			const float Weight =
				FMath::Max(
					0.0f,
					Reward.Weight);

			if (Reward.ItemId.IsNone()
				|| Weight <= 0.0f)
			{
				continue;
			}

			if (Pick > Weight)
			{
				Pick -= Weight;
				continue;
			}

			FTDChestItemReward& Added =
				OutRewards.AddDefaulted_GetRef();

			Added.ItemId = Reward.ItemId;

			const int32 MinCount =
				FMath::Max(
					1,
					Reward.MinCount);

			const int32 MaxCount =
				FMath::Max(
					MinCount,
					Reward.MaxCount);

			Added.Count =
				FMath::RandRange(
					MinCount,
					MaxCount);

			break;
		}
	}
}

bool ATDTreasureChest::CanFitAllItemRewards(
	const TArray<FTDChestItemReward>& Rewards,
	UTDInventoryComponent* Inventory) const
{
	if (Inventory == nullptr)
	{
		return Rewards.IsEmpty();
	}

	TArray<FTDItemInstance> Simulated =
		Inventory->GetItems();

	for (const FTDChestItemReward& Reward :
		Rewards)
	{
		if (Reward.ItemId.IsNone()
			|| Reward.Count <= 0)
		{
			return false;
		}

		const FTDItemRow* Definition =
			Inventory->FindItemDefinition(
				Reward.ItemId);

		if (Definition == nullptr)
		{
			return false;
		}

		const int32 MaxStack =
			Definition->bStackable
				? FMath::Max(
					1,
					Definition->MaxStackSize)
				: 1;

		int32 Remaining = Reward.Count;

		if (Definition->bStackable)
		{
			for (FTDItemInstance& Item :
				Simulated)
			{
				if (Item.ItemId != Reward.ItemId
					|| Item.Count >= MaxStack)
				{
					continue;
				}

				const int32 Added =
					FMath::Min(
						Remaining,
						MaxStack - Item.Count);

				Item.Count += Added;
				Remaining -= Added;

				if (Remaining <= 0)
				{
					break;
				}
			}
		}

		while (Remaining > 0)
		{
			if (Simulated.Num()
				>= Inventory->GetSlotCapacity())
			{
				return false;
			}

			FTDItemInstance NewItem;
			NewItem.ItemId = Reward.ItemId;
			NewItem.Count =
				FMath::Min(
					Remaining,
					MaxStack);

			Remaining -= NewItem.Count;
			Simulated.Add(NewItem);
		}
	}

	return true;
}

void ATDTreasureChest::Interact_Implementation(
	ATDPlayerCharacter* Player)
{
	if (!HasAuthority()
		|| !IsValid(Player))
	{
		return;
	}

	ATDPlayerController* Controller =
		Cast<ATDPlayerController>(
			Player->GetController());

	ATDPlayerState* PlayerState =
		Player->GetPlayerState<ATDPlayerState>();

	const FTDTreasureChestRow* Row =
		GetDefinitionRow();

	const FName ChestId = GetChestId();

	if (Controller == nullptr
		|| PlayerState == nullptr
		|| Row == nullptr
		|| ChestId.IsNone())
	{
		if (Controller != nullptr)
		{
			Controller->ClientInteractionFailed(
				ChestId,
				ETDInteractionFailureReason::
					InvalidDefinition);
		}

		return;
	}

	UTDPersonalWorldStateComponent* Personal =
		PlayerState
			->GetPersonalWorldStateComponent();

	UTDInventoryComponent* Inventory =
		PlayerState->GetInventoryComponent();

	UTDProgressionComponent* Progression =
		PlayerState->GetProgressionComponent();

	UTDQuestComponent* Quest =
		PlayerState->GetQuestComponent();

	if (Personal == nullptr
		|| Inventory == nullptr
		|| Quest == nullptr
		|| (Row->FixedExp > 0
			&& Progression == nullptr))
	{
		Controller->ClientInteractionFailed(
			ChestId,
			ETDInteractionFailureReason::
				InvalidDefinition);

		return;
	}

	if (!Personal->CanClaimChest(
		ChestId,
		Row->ResetType))
	{
		Controller->ClientInteractionFailed(
			ChestId,
			ETDInteractionFailureReason::
				AlreadyClaimed);

		return;
	}

	if (!Personal->MatchesCondition(
		Row->Condition))
	{
		Controller->ClientInteractionFailed(
			ChestId,
			ETDInteractionFailureReason::
				QuestConditionNotMet);

		return;
	}

	/**
	 * 확률 상자는 부분 스택과 관계없이 실제 빈 슬롯 두 칸을 요구한다.
	 */
	if (Row->bUseRandomRewards)
	{
		const int32 EmptySlots =
			Inventory->GetSlotCapacity()
			- Inventory->GetUsedSlotCount();

		if (EmptySlots < 2)
		{
			Controller->ClientInteractionFailed(
				ChestId,
				ETDInteractionFailureReason::
					InventoryFull);

			return;
		}
	}

	TArray<FTDChestItemReward> FinalRewards =
		Row->FixedItemRewards;

	RollRandomRewards(
		*Row,
		FinalRewards);

	if (!CanFitAllItemRewards(
		FinalRewards,
		Inventory))
	{
		Controller->ClientInteractionFailed(
			ChestId,
			ETDInteractionFailureReason::
				InventoryFull);

		return;
	}

	/**
	 * 사전 계산이 끝난 뒤 실제 지급한다.
	 * 서버 함수 한 번이 실행되는 중에는 다른 상호작용 함수가 끼어들지 않는다.
	 */
	for (const FTDChestItemReward& Reward :
		FinalRewards)
	{
		if (!Inventory->AddItem(
			Reward.ItemId,
			Reward.Count))
		{
			UE_LOG(LogTemp, Error,
				TEXT("상자 보상 사전 계산 후 지급 실패: Chest='%s', Item='%s'"),
				*ChestId.ToString(),
				*Reward.ItemId.ToString());

			Controller->ClientInteractionFailed(
				ChestId,
				ETDInteractionFailureReason::
					InvalidDefinition);

			return;
		}
	}

	if (Row->FixedGold > 0)
	{
		Inventory->AddGold(Row->FixedGold);
	}

	if (Row->FixedExp > 0)
	{
		Progression->AddExp(Row->FixedExp);
	}

	if (!Personal->MarkChestClaimedWithReset(
		ChestId,
		Row->ResetType))
	{
		UE_LOG(LogTemp, Error,
			TEXT("상자 보상 지급 후 기록 실패: '%s'"),
			*ChestId.ToString());

		return;
	}

	/**
	 * 꽝이어도 상자가 정상적으로 열렸으므로
	 * 상자 열기 퀘스트는 진행된다.
	 */
	Quest->ReportChestOpened(ChestId);

	Controller->ClientChestClaimed(
		ChestId,
		FMath::Max(
			0.0f,
			Row->DisappearDelay));
}

FText ATDTreasureChest::
GetInteractionText_Implementation(
	ATDPlayerCharacter* Player) const
{
	return NSLOCTEXT(
		"TDInteraction",
		"OpenTreasureChest",
		"상자 열기");
}

void ATDTreasureChest::TryBindToLocalState()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	APlayerController* Controller =
		UGameplayStatics::GetPlayerController(
			this,
			0);

	ATDPlayerState* PlayerState =
		Controller
			? Controller
				->GetPlayerState<ATDPlayerState>()
			: nullptr;

	if (PlayerState == nullptr)
	{
		return;
	}

	if (LocalPersonalState == nullptr)
	{
		LocalPersonalState =
			PlayerState->GetPersonalWorldStateComponent();

		if (LocalPersonalState != nullptr)
		{
			LocalPersonalState
				->OnPersonalWorldStateChanged
				.AddUniqueDynamic(
					this,
					&ATDTreasureChest::HandlePersonalStateChanged);
		}
	}

	if (DailyResetSubsystem == nullptr)
	{
		DailyResetSubsystem =
			GetWorld()->GetSubsystem<
				UTDKoreanDailyResetSubsystem>();

		if (DailyResetSubsystem != nullptr)
		{
			DailyResetSubsystem
				->OnKoreanDayChanged
				.AddUObject(
					this,
					&ATDTreasureChest::
						HandleKoreanDayChanged);
		}
	}

	if (LocalPersonalState != nullptr)
	{
		GetWorldTimerManager().ClearTimer(
			BindRetryTimerHandle);

		RefreshLocalPresentation();
	}
}

void ATDTreasureChest::
HandlePersonalStateChanged()
{
	RefreshLocalPresentation();
}

void ATDTreasureChest::HandleKoreanDayChanged()
{
	bPlayingClaimPresentation = false;
	GetWorldTimerManager().ClearTimer(
		LocalHideTimerHandle);

	RefreshLocalPresentation();
}

void ATDTreasureChest::
RefreshLocalPresentation()
{
	if (LocalPersonalState == nullptr
		|| bPlayingClaimPresentation)
	{
		return;
	}

	const FTDTreasureChestRow* Row =
		GetDefinitionRow();

	const bool bCanClaim =
		Row != nullptr
		&& LocalPersonalState->CanClaimChest(
			GetChestId(),
			Row->ResetType)
		&& LocalPersonalState->MatchesCondition(
			Row->Condition);

	SetLocalPresentationHidden(!bCanClaim);
}

void ATDTreasureChest::PlayClaimedPresentation(
	float DisappearDelay)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	bPlayingClaimPresentation = true;
	SetLocalPresentationHidden(false);

	BP_OnChestClaimed(DisappearDelay);

	GetWorldTimerManager().ClearTimer(
		LocalHideTimerHandle);

	const float SafeDelay =
		FMath::Max(0.0f, DisappearDelay);

	if (SafeDelay <= 0.0f)
	{
		FinishClaimedPresentation();
		return;
	}

	GetWorldTimerManager().SetTimer(
		LocalHideTimerHandle,
		this,
		&ATDTreasureChest::
			FinishClaimedPresentation,
		SafeDelay,
		false);
}

void ATDTreasureChest::
FinishClaimedPresentation()
{
	bPlayingClaimPresentation = false;
	SetLocalPresentationHidden(true);
}

void ATDTreasureChest::SetLocalPresentationHidden(
	bool bShouldHide)
{
	SpriteComponent->SetVisibility(
		!bShouldHide,
		true);

	if (!HasAuthority())
	{
		InteractionSphere->SetCollisionEnabled(
			bShouldHide
				? ECollisionEnabled::NoCollision
				: ECollisionEnabled::QueryOnly);
	}
}