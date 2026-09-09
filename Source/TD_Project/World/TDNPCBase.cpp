#include "World/TDNPCBase.h"

#include "Character/TDPlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDDialogueRow.h"
#include "Data/TDNPCRow.h"
#include "Interaction/TDInteractionFlowComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"
#include "Player/TDPlayerController.h"
#include "Player/TDPlayerState.h"
#include "Quest/TDPersonalWorldStateComponent.h"
#include "Quest/TDQuestComponent.h"
#include "UI/HUD/TDQuestMarkerWidget.h"
#include "World/TDKoreanDailyResetSubsystem.h"

ATDNPCBase::ATDNPCBase()
{
	/**
	 * 각 클라이언트가 자기 캐릭터 위치를 기준으로
	 * NPC의 화면상 방향을 계산한다.
	 */
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	/**
	 * NPC 자체는 복제할 수 있게 유지한다.
	 *
	 * 하지만 회전은 플레이어마다 다르게 보여야 하므로
	 * Transform과 Movement는 복제하지 않는다.
	 */
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot =
		CreateDefaultSubobject<USceneComponent>(
			TEXT("SceneRoot"));

	SceneRoot->SetCanEverAffectNavigation(false);
	
	SetRootComponent(SceneRoot);

	SpriteComponent =
		CreateDefaultSubobject<UPaperFlipbookComponent>(
			TEXT("SpriteComponent"));

	SpriteComponent->SetCanEverAffectNavigation(false);
	
	SpriteComponent->SetupAttachment(SceneRoot);

	SpriteComponent->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);

	SpriteComponent->SetGenerateOverlapEvents(false);

	/**
	 * NPC 액터가 회전해도 2D 스프라이트 판 자체는
	 * 카메라 방향을 유지한다.
	 *
	 * 액터 회전값은 PaperZD 방향 선택에만 사용한다.
	 */
	SpriteComponent->SetUsingAbsoluteRotation(true);

	AnimationComponent =
		CreateDefaultSubobject<
			UPaperZDAnimationComponent>(
				TEXT("AnimationComponent"));

	AnimationComponent->SetCanEverAffectNavigation(false);
	
	AnimationComponent->InitRenderComponent(
		SpriteComponent);

	InteractionSphere =
		CreateDefaultSubobject<USphereComponent>(
			TEXT("InteractionSphere"));

	InteractionSphere->SetCanEverAffectNavigation(false);
	
	InteractionSphere->SetupAttachment(SceneRoot);

	InteractionSphere->SetSphereRadius(150.0f);

	InteractionSphere->SetCollisionObjectType(
		ECC_WorldDynamic);

	InteractionSphere->SetCollisionEnabled(
		ECollisionEnabled::QueryOnly);

	InteractionSphere
		->SetCollisionResponseToAllChannels(
			ECR_Ignore);

	InteractionSphere
		->SetCollisionResponseToChannel(
			ECC_Pawn,
			ECR_Overlap);

	QuestMarkerComponent =
		CreateDefaultSubobject<UWidgetComponent>(
			TEXT("QuestMarkerComponent"));

	QuestMarkerComponent->SetCanEverAffectNavigation(false);
	
	QuestMarkerComponent->SetupAttachment(SceneRoot);

	QuestMarkerComponent->SetWidgetSpace(
		EWidgetSpace::Screen);

	QuestMarkerComponent->SetDrawSize(
		FVector2D(80.0f, 80.0f));

	QuestMarkerComponent->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);

	QuestMarkerComponent->SetVisibility(false);
}

void ATDNPCBase::BeginPlay()
{
	Super::BeginPlay();

	/**
	 * 맵에 배치된 NPC 회전에 PaperZD 방향 보정값을 더한다.
	 *
	 * 플레이어를 바라볼 때와 가만히 있을 때 모두
	 * 같은 좌표 기준을 사용해야 방향이 일치한다.
	 */
	InitialFacingRotation =
		FRotator(
			0.0f,
			GetActorRotation().Yaw
				- FacingYawOffsetDegrees,
			0.0f);

	InitialFacingRotation.Normalize();

	/**
	 * 첫 Tick을 기다리지 않고 처음부터
	 * 올바른 기본 방향으로 표시한다.
	 */
	SetActorRotation(
		InitialFacingRotation);

	if (GetDefinitionRow() == nullptr)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"NPC '%s': NPCDefinition을 확인하세요."),
			*GetName());
	}

	if (DialogueTable == nullptr)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"NPC '%s': DialogueTable이 없습니다."),
			*GetName());
	}

	/**
	 * 전용 서버에는 화면과 로컬 플레이어가 없으므로
	 * NPC 방향 계산이 필요 없다.
	 */
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return;
	}

	TryBindToLocalPlayerState();

	if (LocalPersonalState == nullptr
		|| LocalQuestComponent == nullptr)
	{
		GetWorldTimerManager().SetTimer(
			BindRetryTimerHandle,
			this,
			&ATDNPCBase::
				TryBindToLocalPlayerState,
			0.25f,
			true);
	}
}

void ATDNPCBase::Tick(
	float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	/**
	 * 전용 서버에는 로컬 화면이 없기 때문에
	 * 방향 표현을 계산하지 않는다.
	 */
	if (GetNetMode() == NM_DedicatedServer
		|| GetWorld() == nullptr)
	{
		return;
	}

	/**
	 * 이 컴퓨터에서 직접 조작하는 플레이어만 가져온다.
	 *
	 * 다른 파티원이나 다른 네트워크 플레이어는
	 * 방향 계산 대상에 포함되지 않는다.
	 */
	ATDPlayerCharacter* LocalPlayer =
		Cast<ATDPlayerCharacter>(
			UGameplayStatics::GetPlayerCharacter(
				this,
				0));

	FRotator DesiredRotation =
		InitialFacingRotation;

	const bool bCanLookAtLocalPlayer =
		IsValid(LocalPlayer)
		&& !LocalPlayer->IsDead();

	if (bCanLookAtLocalPlayer)
	{
		const float DistanceSquared =
			FVector::DistSquared2D(
				GetActorLocation(),
				LocalPlayer->GetActorLocation());

		const float LookAtDistance =
			FMath::Max(
				0.0f,
				PlayerLookAtDistance);

		const bool bLocalPlayerIsNear =
			DistanceSquared
			<= FMath::Square(LookAtDistance);

		if (bLocalPlayerIsNear)
		{
			FVector DirectionToPlayer =
				LocalPlayer->GetActorLocation()
				- GetActorLocation();

			/**
			 * 플레이어와 NPC의 높이 차이는
			 * 바라보는 방향에 사용하지 않는다.
			 */
			DirectionToPlayer.Z = 0.0f;

			if (DirectionToPlayer.Normalize())
			{
				const float DesiredYaw =
					DirectionToPlayer.Rotation().Yaw
					+ FacingYawOffsetDegrees;

				DesiredRotation =
					FRotator(
						0.0f,
						DesiredYaw,
						0.0f);
			}
		}
		else if (!bReturnToInitialFacing)
		{
			/**
			 * 플레이어가 멀리 있고 원래 방향으로
			 * 돌아가는 기능도 꺼져 있으면
			 * 현재 방향을 유지한다.
			 */
			return;
		}
	}
	else if (!bReturnToInitialFacing)
	{
		return;
	}

	const FRotator CurrentRotation =
		GetActorRotation();

	/**
	 * 현재 방향에서 목표 방향으로 서서히 회전한다.
	 */
	FRotator NewRotation =
		FMath::RInterpTo(
			CurrentRotation,
			DesiredRotation,
			DeltaSeconds,
			FMath::Max(
				0.1f,
				FacingRotationInterpSpeed));

	NewRotation.Pitch = 0.0f;
	NewRotation.Roll = 0.0f;
	NewRotation.Normalize();

	if (CurrentRotation.Equals(
		NewRotation,
		0.05f))
	{
		return;
	}

	/**
	 * 이 회전은 현재 클라이언트의 화면에만 적용된다.
	 *
	 * Replicate Movement가 꺼져 있기 때문에
	 * 서버나 다른 플레이어 화면으로 전달되지 않는다.
	 */
	SetActorRotation(NewRotation);
}

void ATDNPCBase::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(
		BindRetryTimerHandle);

	if (LocalPersonalState != nullptr)
	{
		LocalPersonalState
			->OnPersonalWorldStateChanged
			.RemoveDynamic(
				this,
				&ATDNPCBase::HandleLocalStateChanged);
	}

	if (LocalQuestComponent != nullptr)
	{
		LocalQuestComponent
			->OnQuestListChanged
			.RemoveDynamic(
				this,
				&ATDNPCBase::HandleLocalStateChanged);

		LocalQuestComponent
			->OnAffectionChanged
			.RemoveAll(this);
	}

	if (DailyResetSubsystem != nullptr)
	{
		DailyResetSubsystem
			->OnKoreanDayChanged
			.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

const FTDNPCRow* ATDNPCBase::GetDefinitionRow() const
{
	if (NPCDefinition.DataTable == nullptr
		|| NPCDefinition.RowName.IsNone())
	{
		return nullptr;
	}

	return NPCDefinition.GetRow<FTDNPCRow>(
		TEXT("ATDNPCBase"));
}

FText ATDNPCBase::GetNPCDisplayName() const
{
	const FTDNPCRow* Row = GetDefinitionRow();

	return Row
		? Row->DisplayName
		: FText::FromName(GetNPCId());
}

TSoftObjectPtr<UTexture2D>
ATDNPCBase::GetNPCPortrait() const
{
	const FTDNPCRow* Row = GetDefinitionRow();

	return Row
		? Row->Portrait
		: TSoftObjectPtr<UTexture2D>();
}

bool ATDNPCBase::IsPlayerWithinDialogueDistance(
	const AActor* PlayerActor) const
{
	return IsValid(PlayerActor)
		&& FVector::DistSquared(
			GetActorLocation(),
			PlayerActor->GetActorLocation())
		<= FMath::Square(
			FMath::Max(
				150.0f,
				DialogueContinueDistance));
}

bool ATDNPCBase::CanReceiveGifts() const
{
	const FTDNPCRow* Row = GetDefinitionRow();
	return Row != nullptr && Row->bAcceptsGifts;
}

int32 ATDNPCBase::GetGiftAffectionValue(
	FName ItemId) const
{
	const FTDNPCRow* Row = GetDefinitionRow();

	if (Row == nullptr || ItemId.IsNone())
	{
		return 0;
	}

	const FTDNPCGiftPreference* Preference =
		Row->GiftPreferences.FindByPredicate(
			[ItemId](
				const FTDNPCGiftPreference& Entry)
			{
				return Entry.ItemId == ItemId;
			});

	return Preference
		? FMath::Max(
			0,
			Preference->AffectionGain)
		: 0;
}

FText ATDNPCBase::GetGiftThankYouText() const
{
	const FTDNPCRow* Row = GetDefinitionRow();

	if (Row != nullptr
		&& !Row->GiftThankYouText.IsEmpty())
	{
		return Row->GiftThankYouText;
	}

	return NSLOCTEXT(
		"TDAffection",
		"DefaultThanks",
		"고마워.");
}

FName ATDNPCBase::FindInProgressDialogueRow(
	const UTDQuestComponent* Quest) const
{
	if (Quest == nullptr)
	{
		return NAME_None;
	}

	/*
	 * GetQuestViews는 진행 중인 퀘스트를
	 * 메인 우선, 이후 수락 순서로 반환한다.
	 *
	 * 여기서는 메인을 제외하고
	 * 서브/일일 퀘스트의 진행 안내만 찾는다.
	 */
	const TArray<FTDQuestViewData> Views =
		Quest->GetQuestViews();

	for (const FTDQuestViewData& View : Views)
	{
		/*
		 * 메인의 실제 대화 목표와 완료 보고는
		 * ResolveDialogueStartRow에서 별도로 처리한다.
		 *
		 * 이곳에서 메인을 다시 선택하면
		 * 사냥 대기 멘트가 서브 대화를 막게 된다.
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
				ETDQuestTargetType::NPC
			&& Definition->AcceptTargetId == GetNPCId();

		const bool bMatchesTurnInTarget =
			Definition->TurnInTargetType ==
				ETDQuestTargetType::NPC
			&& Definition->TurnInTargetId == GetNPCId();

		if (bMatchesAcceptTarget || bMatchesTurnInTarget)
		{
			return Definition->InProgressDialogueRow;
		}
	}

	return NAME_None;
}

FName ATDNPCBase::ResolveDialogueStartRow(
	const ATDPlayerCharacter* Player) const
{
	const FTDNPCRow* Row = GetDefinitionRow();

	const ATDPlayerState* PlayerState =
		Player
			? Player->GetPlayerState<ATDPlayerState>()
			: nullptr;

	const UTDQuestComponent* Quest =
		PlayerState
			? PlayerState->GetQuestComponent()
			: nullptr;

	const UTDPersonalWorldStateComponent* Personal =
		PlayerState
			? PlayerState->GetPersonalWorldStateComponent()
			: nullptr;

	if (Row == nullptr
		|| Quest == nullptr
		|| Personal == nullptr)
	{
		return NAME_None;
	}

	const FName CurrentNpcId = GetNPCId();

	const auto IsMainDefinition =
		[](const FTDQuestRow* Definition)
		{
			return Definition != nullptr
				&& Definition->QuestTypeTag ==
					TDTags::Quest_Type_Main.GetTag();
		};

	// 1. 현재 NPC에게 완료 보고할 수 있는 메인을 우선한다.
	const FName TurnInQuestId =
		Quest->FindBestTurnInQuestForTarget(
			ETDQuestTargetType::NPC,
			CurrentNpcId);

	const FTDQuestRow* TurnInDefinition =
		TurnInQuestId.IsNone()
			? nullptr
			: Quest->GetQuestDefinition(TurnInQuestId);

	if (IsMainDefinition(TurnInDefinition))
	{
		return TurnInDefinition->TurnInDialogueRow;
	}

	// 2. 현재 NPC와 대화하는 것이 목표인 메인을 처리한다.
	// 사냥 중인 메인의 단순 안내 대사는 여기서 선택하지 않는다.
	const FName ActiveMainQuestId =
		Quest->FindActiveMainQuestForTarget(
			ETDQuestTargetType::NPC,
			CurrentNpcId);

	if (!ActiveMainQuestId.IsNone())
	{
		const FTDQuestRow* ActiveMainDefinition =
			Quest->GetQuestDefinition(ActiveMainQuestId);

		return ActiveMainDefinition != nullptr
			? ActiveMainDefinition->InProgressDialogueRow
			: NAME_None;
	}

	// 3. 현재 NPC가 제안할 수 있는 퀘스트를 찾는다.
	// 슬롯이 가득 찼어도 제안 자체는 보여주고,
	// 실제 수락 시 서버가 2개 제한을 검사한다.
	const FName OfferQuestId =
		Quest->FindBestOfferQuestForTarget(
			ETDQuestTargetType::NPC,
			CurrentNpcId,
			true);

	const FTDQuestRow* OfferDefinition =
		OfferQuestId.IsNone()
			? nullptr
			: Quest->GetQuestDefinition(OfferQuestId);

	if (IsMainDefinition(OfferDefinition))
	{
		return OfferDefinition->OfferDialogueRow;
	}

	// 4. 완료 보고 가능한 서브/일일 퀘스트.
	if (TurnInDefinition != nullptr
		&& !TurnInDefinition->TurnInDialogueRow.IsNone())
	{
		return TurnInDefinition->TurnInDialogueRow;
	}

	// 5. 조건에 맞는 특별 대화.
	// 한수현/서아영에게 점심 메뉴를 묻는 대화가 여기에 해당한다.
	for (const FTDNPCDialogueRule& Rule : Row->DialogueRules)
	{
		if (!Rule.StartDialogueRow.IsNone()
			&& Personal->MatchesCondition(Rule.Condition))
		{
			return Rule.StartDialogueRow;
		}
	}

	// 6. 새로운 서브/일일 퀘스트 제안.
	// 단순 진행 안내보다 먼저 선택해야,
	// 같은 NPC에게 두 번째 서브도 받을 수 있다.
	if (OfferDefinition != nullptr
		&& !OfferDefinition->OfferDialogueRow.IsNone())
	{
		return OfferDefinition->OfferDialogueRow;
	}

	// 7. 이미 받은 서브/일일 퀘스트의 단순 진행 안내.
	const FName InProgressRow =
		FindInProgressDialogueRow(Quest);

	if (!InProgressRow.IsNone())
	{
		return InProgressRow;
	}

	// 8. 처리할 퀘스트나 특별 대화가 없으면 기본 대사.
	return Row->DefaultDialogueRow;
}

bool ATDNPCBase::CanInteract_Implementation(
	ATDPlayerCharacter* Player) const
{
	if (!IsValid(Player)
		|| !IsPlayerWithinDialogueDistance(Player)
		|| DialogueTable == nullptr)
	{
		return false;
	}

	const FTDNPCRow* Row = GetDefinitionRow();

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
			Row->VisibilityCondition))
	{
		return false;
	}

	const FName StartRow =
		ResolveDialogueStartRow(Player);

	return !StartRow.IsNone()
		&& DialogueTable->FindRow<FTDDialogueRow>(
			StartRow,
			TEXT("ATDNPCBase::CanInteract"),
			false) != nullptr;
}

void ATDNPCBase::Interact_Implementation(
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
		/**
		 * NPC 방향 변경은 Tick에서 처리한다.
		 *
		 * 이 함수는 대화 시작만 담당한다.
		 */
		Flow->BeginDialogueFromSource(
			this,
			ResolveDialogueStartRow(Player));
	}
}

FText ATDNPCBase::
GetInteractionText_Implementation(
	ATDPlayerCharacter* Player) const
{
	const FTDNPCRow* Row = GetDefinitionRow();

	if (Row != nullptr
		&& !Row->InteractionText.IsEmpty())
	{
		return Row->InteractionText;
	}

	return NSLOCTEXT(
		"TDInteraction",
		"TalkToNPC",
		"대화하기");
}

FName ATDNPCBase::
GetDialogueSourceId_Implementation() const
{
	return GetNPCId();
}

ETDQuestTargetType ATDNPCBase::
GetDialogueQuestTargetType_Implementation() const
{
	return ETDQuestTargetType::NPC;
}

FText ATDNPCBase::
GetDialogueDisplayName_Implementation() const
{
	return GetNPCDisplayName();
}

TSoftObjectPtr<UTexture2D>
ATDNPCBase::GetDialoguePortrait_Implementation() const
{
	return GetNPCPortrait();
}

UDataTable* ATDNPCBase::
GetDialogueTable_Implementation() const
{
	return DialogueTable;
}

bool ATDNPCBase::
IsDialogueSourceInRange_Implementation(
	const AActor* PlayerActor) const
{
	return IsPlayerWithinDialogueDistance(
		PlayerActor);
}

void ATDNPCBase::TryBindToLocalPlayerState()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	APlayerController* LocalController =
		UGameplayStatics::GetPlayerController(
			this,
			0);

	ATDPlayerState* PlayerState =
		LocalController
			? LocalController
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
					&ATDNPCBase::HandleLocalStateChanged);
		}
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
					&ATDNPCBase::HandleLocalStateChanged);
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
					&ATDNPCBase::
						HandleKoreanDayChanged);
		}
	}

	if (LocalPersonalState != nullptr
		&& LocalQuestComponent != nullptr)
	{
		GetWorldTimerManager().ClearTimer(
			BindRetryTimerHandle);

		RefreshLocalPresentation();
	}
}

void ATDNPCBase::HandleLocalStateChanged()
{
	RefreshLocalPresentation();
}

void ATDNPCBase::HandleKoreanDayChanged()
{
	RefreshLocalPresentation();
}

void ATDNPCBase::SetQuestMarkerSuppressed(
	bool bSuppressed)
{
	if (bQuestMarkerSuppressed == bSuppressed)
	{
		return;
	}

	bQuestMarkerSuppressed = bSuppressed;

	if (bQuestMarkerSuppressed)
	{
		QuestMarkerComponent->SetVisibility(false);
		return;
	}

	/**
	 * 대화가 종료되었으면 현재 퀘스트 상태를 다시 검사한다.
	 *
	 * ESC로 대화를 닫았다면 기존 !가 다시 나타나고,
	 * 퀘스트를 완료했다면 이전 NPC 마커는 사라지며,
	 * 다음 퀘스트 NPC의 !가 나타난다.
	 */
	RefreshLocalPresentation();
}

void ATDNPCBase::RefreshLocalPresentation()
{
	if (GetNetMode() == NM_DedicatedServer
		|| LocalPersonalState == nullptr)
	{
		return;
	}

	const FTDNPCRow* Row = GetDefinitionRow();

	const bool bVisible =
		Row != nullptr
		&& LocalPersonalState->MatchesCondition(
			Row->VisibilityCondition);

	SetLocalPresentationHidden(!bVisible);

	if (!bVisible
		|| bQuestMarkerSuppressed
		|| LocalQuestComponent == nullptr
		|| Row == nullptr
		|| !Row->bShowQuestMarker)
	{
		QuestMarkerComponent->SetVisibility(false);
		return;
	}

	const FTDQuestMarkerView Marker =
		LocalQuestComponent
			->GetQuestMarkerForTarget(
				ETDQuestTargetType::NPC,
				GetNPCId());

	const bool bShowMarker =
		Marker.MarkerType !=
			ETDQuestMarkerType::None;

	QuestMarkerComponent->SetVisibility(
		bShowMarker);

	if (bShowMarker)
	{
		if (UTDQuestMarkerWidget* MarkerWidget =
			Cast<UTDQuestMarkerWidget>(
				QuestMarkerComponent
					->GetUserWidgetObject()))
		{
			MarkerWidget->SetMarkerData(Marker);
		}
	}
}

void ATDNPCBase::SetLocalPresentationHidden(
	bool bShouldHide)
{
	SpriteComponent->SetVisibility(
		!bShouldHide,
		true);

	AnimationComponent->SetComponentTickEnabled(
		!bShouldHide);

	QuestMarkerComponent->SetVisibility(false);

	if (!HasAuthority())
	{
		InteractionSphere->SetCollisionEnabled(
			bShouldHide
				? ECollisionEnabled::NoCollision
				: ECollisionEnabled::QueryOnly);
	}
}