#include "World/TDQuestObject.h"

#include "Character/TDPlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDDialogueRow.h"
#include "Data/TDQuestObjectRow.h"
#include "Kismet/GameplayStatics.h"
#include "PaperFlipbookComponent.h"
#include "Player/TDPlayerState.h"
#include "Quest/TDPersonalWorldStateComponent.h"
#include "Quest/TDQuestComponent.h"
#include "UI/HUD/TDQuestMarkerWidget.h"
#include "World/TDKoreanDailyResetSubsystem.h"
#include "Interaction/TDInteractionFlowComponent.h"

ATDQuestObject::ATDQuestObject()
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

	QuestMarkerComponent =
		CreateDefaultSubobject<UWidgetComponent>(
			TEXT("QuestMarkerComponent"));

	QuestMarkerComponent->SetupAttachment(SceneRoot);
	QuestMarkerComponent->SetWidgetSpace(
		EWidgetSpace::Screen);
	QuestMarkerComponent->SetDrawSize(
		FVector2D(80.0f, 80.0f));
	QuestMarkerComponent->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	QuestMarkerComponent->SetVisibility(false);
}

void ATDQuestObject::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	TryBindToLocalPlayerState();

	if (LocalQuestComponent == nullptr
		|| LocalPersonalState == nullptr)
	{
		GetWorldTimerManager().SetTimer(
			BindRetryTimerHandle,
			this,
			&ATDQuestObject::
				TryBindToLocalPlayerState,
			0.25f,
			true);
	}
}

void ATDQuestObject::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(
		BindRetryTimerHandle);

	if (LocalQuestComponent != nullptr)
	{
		LocalQuestComponent
			->OnQuestListChanged
			.RemoveDynamic(
				this,
				&ATDQuestObject::HandleLocalStateChanged);
	}

	if (LocalPersonalState != nullptr)
	{
		LocalPersonalState
			->OnPersonalWorldStateChanged
			.RemoveDynamic(
				this,
				&ATDQuestObject::HandleLocalStateChanged);
	}

	if (DailyResetSubsystem != nullptr)
	{
		DailyResetSubsystem
			->OnKoreanDayChanged
			.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

const FTDQuestObjectRow*
ATDQuestObject::GetDefinitionRow() const
{
	if (ObjectDefinition.DataTable == nullptr
		|| ObjectDefinition.RowName.IsNone())
	{
		return nullptr;
	}

	return ObjectDefinition.GetRow<
		FTDQuestObjectRow>(
			TEXT("ATDQuestObject"));
}

FName ATDQuestObject::FindInProgressDialogueRow(
	const UTDQuestComponent* Quest) const
{
	if (Quest == nullptr)
	{
		return NAME_None;
	}

	const TArray<FTDQuestViewData> Views =
		Quest->GetQuestViews();

	for (const FTDQuestViewData& View : Views)
	{
		/*
		 * 메인 대화 목표와 완료 보고는 별도로 처리한다.
		 * 일반 진행 안내에서는 메인을 제외한다.
		 */
		if (View.QuestTypeTag ==
			TDTags::Quest_Type_Main.GetTag())
		{
			continue;
		}

		const FTDQuestRow* Definition =
			Quest->GetQuestDefinition(View.QuestId);

		if (Definition == nullptr
			|| Definition->InProgressDialogueRow.IsNone())
		{
			continue;
		}

		const bool bMatchesAcceptTarget =
			Definition->AcceptTargetType ==
				ETDQuestTargetType::QuestObject
			&& Definition->AcceptTargetId == GetQuestObjectId();

		const bool bMatchesTurnInTarget =
			Definition->TurnInTargetType ==
				ETDQuestTargetType::QuestObject
			&& Definition->TurnInTargetId == GetQuestObjectId();

		if (bMatchesAcceptTarget || bMatchesTurnInTarget)
		{
			return Definition->InProgressDialogueRow;
		}
	}

	return NAME_None;
}

FName ATDQuestObject::ResolveDialogueStartRow(
	const ATDPlayerCharacter* Player) const
{
	const FTDQuestObjectRow* Row =
		GetDefinitionRow();

	const ATDPlayerState* PlayerState =
		Player
			? Player->GetPlayerState<ATDPlayerState>()
			: nullptr;

	const UTDQuestComponent* Quest =
		PlayerState
			? PlayerState->GetQuestComponent()
			: nullptr;

	if (Row == nullptr || Quest == nullptr)
	{
		return NAME_None;
	}

	const auto IsMainDefinition =
		[](const FTDQuestRow* Definition)
		{
			return Definition != nullptr
				&& Definition->QuestTypeTag ==
					TDTags::Quest_Type_Main.GetTag();
		};

	/*
	 * 1. 완료 가능한 메인 퀘스트
	 */
	const FName TurnInQuestId =
		Quest->FindBestTurnInQuestForTarget(
			ETDQuestTargetType::QuestObject,
			GetQuestObjectId());

	const FTDQuestRow* TurnInDefinition =
		TurnInQuestId.IsNone()
			? nullptr
			: Quest->GetQuestDefinition(
				TurnInQuestId);

	if (IsMainDefinition(TurnInDefinition))
	{
		return !TurnInDefinition
			->TurnInDialogueRow.IsNone()
				? TurnInDefinition
					->TurnInDialogueRow
				: NAME_None;
	}

	/*
	 * 2. 현재 물건과 관련된 진행 중 메인 퀘스트
	 */
	const FName ActiveMainQuestId =
		Quest->FindActiveMainQuestForTarget(
			ETDQuestTargetType::QuestObject,
			GetQuestObjectId());

	if (!ActiveMainQuestId.IsNone())
	{
		const FTDQuestRow* ActiveMainDefinition =
			Quest->GetQuestDefinition(
				ActiveMainQuestId);

		return ActiveMainDefinition != nullptr
			&& !ActiveMainDefinition
				->InProgressDialogueRow.IsNone()
					? ActiveMainDefinition
						->InProgressDialogueRow
					: NAME_None;
	}

	/*
	 * 3. 받을 수 있는 메인 퀘스트
	 */
	const FName OfferQuestId =
		Quest->FindBestOfferQuestForTarget(
			ETDQuestTargetType::QuestObject,
			GetQuestObjectId(),
			true);

	const FTDQuestRow* OfferDefinition =
		OfferQuestId.IsNone()
			? nullptr
			: Quest->GetQuestDefinition(
				OfferQuestId);

	if (IsMainDefinition(OfferDefinition))
	{
		return !OfferDefinition
			->OfferDialogueRow.IsNone()
				? OfferDefinition
					->OfferDialogueRow
				: NAME_None;
	}

	/*
	 * 4. 완료 가능한 서브/일일 퀘스트
	 */
	if (TurnInDefinition != nullptr
		&& !TurnInDefinition
			->TurnInDialogueRow.IsNone())
	{
		return TurnInDefinition
			->TurnInDialogueRow;
	}

	/*
	 * 5. 이미 수락한 서브/일일 퀘스트
	 */
	const FName InProgressRow =
		FindInProgressDialogueRow(Quest);

	if (!InProgressRow.IsNone())
	{
		return InProgressRow;
	}

	/*
	 * 6. 받을 수 있는 서브/일일 퀘스트
	 */
	if (OfferDefinition != nullptr
		&& !OfferDefinition
			->OfferDialogueRow.IsNone())
	{
		return OfferDefinition
			->OfferDialogueRow;
	}

	return Row->DefaultDialogueRow;
}

bool ATDQuestObject::HasRelevantQuest(
	const UTDQuestComponent* Quest) const
{
	if (Quest == nullptr)
	{
		return false;
	}

	if (!Quest->FindBestTurnInQuestForTarget(
			ETDQuestTargetType::QuestObject,
			GetQuestObjectId()).IsNone())
	{
		return true;
	}

	if (!Quest->FindBestOfferQuestForTarget(
			ETDQuestTargetType::QuestObject,
			GetQuestObjectId(),
			true).IsNone())
	{
		return true;
	}

	for (const FTDQuestViewData& View :
		Quest->GetQuestViews())
	{
		const FTDQuestRow* Definition =
			Quest->GetQuestDefinition(
				View.QuestId);

		if (Definition == nullptr)
		{
			continue;
		}

		if ((Definition->AcceptTargetType ==
					ETDQuestTargetType::QuestObject
				&& Definition->AcceptTargetId ==
					GetQuestObjectId())
			|| (Definition->TurnInTargetType ==
					ETDQuestTargetType::QuestObject
				&& Definition->TurnInTargetId ==
					GetQuestObjectId()))
		{
			return true;
		}
	}

	return false;
}

bool ATDQuestObject::CanInteract_Implementation(
	ATDPlayerCharacter* Player) const
{
	if (!IsValid(Player)
		|| DialogueTable == nullptr)
	{
		return false;
	}

	const FTDQuestObjectRow* Row =
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
			Row->VisibilityCondition)
		|| !IsDialogueSourceInRange_Implementation(
			Player))
	{
		return false;
	}

	const FName StartRow =
		ResolveDialogueStartRow(Player);

	return !StartRow.IsNone()
		&& DialogueTable->FindRow<FTDDialogueRow>(
			StartRow,
			TEXT("ATDQuestObject::CanInteract"),
			false) != nullptr;
}

void ATDQuestObject::Interact_Implementation(
	ATDPlayerCharacter* Player)
{
	if (!HasAuthority()
		|| !CanInteract_Implementation(Player))
	{
		return;
	}

	APlayerController* Controller =
		Cast<APlayerController>(
			Player->GetController());

	UTDInteractionFlowComponent* Flow =
		Controller
			? Controller->FindComponentByClass<
				UTDInteractionFlowComponent>()
			: nullptr;

	if (Flow != nullptr)
	{
		Flow->BeginDialogueFromSource(
			this,
			ResolveDialogueStartRow(Player));
	}
}

FText ATDQuestObject::
GetInteractionText_Implementation(
	ATDPlayerCharacter* Player) const
{
	const FTDQuestObjectRow* Row =
		GetDefinitionRow();

	if (Row != nullptr
		&& !Row->InteractionText.IsEmpty())
	{
		return Row->InteractionText;
	}

	return NSLOCTEXT(
		"TDInteraction",
		"InspectQuestObject",
		"살펴보기");
}

FName ATDQuestObject::
GetDialogueSourceId_Implementation() const
{
	return GetQuestObjectId();
}

ETDQuestTargetType ATDQuestObject::
GetDialogueQuestTargetType_Implementation() const
{
	return ETDQuestTargetType::QuestObject;
}

FText ATDQuestObject::
GetDialogueDisplayName_Implementation() const
{
	const FTDQuestObjectRow* Row =
		GetDefinitionRow();

	return Row
		? Row->DisplayName
		: FText::FromName(GetQuestObjectId());
}

TSoftObjectPtr<UTexture2D>
ATDQuestObject::
GetDialoguePortrait_Implementation() const
{
	const FTDQuestObjectRow* Row =
		GetDefinitionRow();

	return Row
		? Row->Portrait
		: TSoftObjectPtr<UTexture2D>();
}

UDataTable* ATDQuestObject::
GetDialogueTable_Implementation() const
{
	return DialogueTable;
}

bool ATDQuestObject::
IsDialogueSourceInRange_Implementation(
	const AActor* PlayerActor) const
{
	const FTDQuestObjectRow* Row =
		GetDefinitionRow();

	if (!IsValid(PlayerActor) || Row == nullptr)
	{
		return false;
	}

	return FVector::DistSquared(
		GetActorLocation(),
		PlayerActor->GetActorLocation())
		<= FMath::Square(
			FMath::Max(
				150.0f,
				Row->DialogueDistance));
}

void ATDQuestObject::TryBindToLocalPlayerState()
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

	if (LocalQuestComponent == nullptr)
	{
		LocalQuestComponent =
			PlayerState->GetQuestComponent();

		if (LocalQuestComponent != nullptr)
		{
			LocalQuestComponent
				->OnQuestListChanged
				.AddUniqueDynamic(
					this,
					&ATDQuestObject::HandleLocalStateChanged);
		}
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
					&ATDQuestObject::HandleLocalStateChanged);
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
					&ATDQuestObject::
						HandleKoreanDayChanged);
		}
	}

	if (LocalQuestComponent != nullptr
		&& LocalPersonalState != nullptr)
	{
		GetWorldTimerManager().ClearTimer(
			BindRetryTimerHandle);

		RefreshLocalPresentation();
	}
}

void ATDQuestObject::HandleLocalStateChanged()
{
	RefreshLocalPresentation();
}

void ATDQuestObject::HandleKoreanDayChanged()
{
	RefreshLocalPresentation();
}

void ATDQuestObject::RefreshLocalPresentation()
{
	const FTDQuestObjectRow* Row =
		GetDefinitionRow();

	if (Row == nullptr
		|| LocalQuestComponent == nullptr
		|| LocalPersonalState == nullptr)
	{
		return;
	}

	bool bVisible =
		LocalPersonalState->MatchesCondition(
			Row->VisibilityCondition);

	if (bVisible
		&& Row->bHideWhenNoRelevantQuest)
	{
		bVisible =
			HasRelevantQuest(
				LocalQuestComponent);
	}

	SetLocalPresentationHidden(!bVisible);

	if (!bVisible || !Row->bShowQuestMarker)
	{
		QuestMarkerComponent->SetVisibility(false);
		return;
	}

	const FTDQuestMarkerView Marker =
		LocalQuestComponent
			->GetQuestMarkerForTarget(
				ETDQuestTargetType::QuestObject,
				GetQuestObjectId());

	const bool bShow =
		Marker.MarkerType !=
			ETDQuestMarkerType::None;

	QuestMarkerComponent->SetVisibility(bShow);

	if (bShow)
	{
		if (UTDQuestMarkerWidget* Widget =
			Cast<UTDQuestMarkerWidget>(
				QuestMarkerComponent
					->GetUserWidgetObject()))
		{
			Widget->SetMarkerData(Marker);
		}
	}
}

void ATDQuestObject::SetLocalPresentationHidden(
	bool bShouldHide)
{
	SpriteComponent->SetVisibility(
		!bShouldHide,
		true);

	QuestMarkerComponent->SetVisibility(false);

	if (!HasAuthority())
	{
		InteractionSphere->SetCollisionEnabled(
			bShouldHide
				? ECollisionEnabled::NoCollision
				: ECollisionEnabled::QueryOnly);
	}
}