#include "World/TDNPCBase.h"

#include "Character/TDPlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Data/TDDialogueRow.h"
#include "Data/TDNPCRow.h"
#include "Kismet/GameplayStatics.h"
#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"
#include "Player/TDPlayerController.h"
#include "Player/TDPlayerState.h"
#include "Quest/TDPersonalWorldStateComponent.h"
#include "TimerManager.h"

ATDNPCBase::ATDNPCBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// 레벨에 고정 배치하며 플레이어별 표시 상태는 각 클라이언트가 계산한다.
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
	SpriteComponent->SetGenerateOverlapEvents(false);
	SpriteComponent->SetUsingAbsoluteRotation(true);

	AnimationComponent =
		CreateDefaultSubobject<UPaperZDAnimationComponent>(
			TEXT("AnimationComponent"));

	InteractionSphere =
		CreateDefaultSubobject<USphereComponent>(
			TEXT("InteractionSphere"));

	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->SetSphereRadius(150.0f);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetCollisionEnabled(
		ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(
		ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(
		ECC_Pawn,
		ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);
}

void ATDNPCBase::BeginPlay()
{
	Super::BeginPlay();

	if (GetDefinitionRow() == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("NPC '%s': NPCDefinition이 없거나 DT_NPC 행을 찾지 못했다."),
			*GetName());
	}

	if (DialogueTable == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("NPC '%s': DialogueTable이 지정되지 않았다."),
			*GetName());
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	TryBindToLocalPersonalState();

	if (LocalPersonalState == nullptr)
	{
		GetWorldTimerManager().SetTimer(
			BindRetryTimerHandle,
			this,
			&ATDNPCBase::TryBindToLocalPersonalState,
			0.25f,
			true);
	}
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
				&ATDNPCBase::HandlePersonalWorldStateChanged);
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
	if (!IsValid(PlayerActor))
	{
		return false;
	}

	return FVector::DistSquared(
		GetActorLocation(),
		PlayerActor->GetActorLocation())
		<= FMath::Square(
			FMath::Max(150.0f, DialogueContinueDistance));
}

FName ATDNPCBase::ResolveDialogueStartRow(
	const ATDPlayerCharacter* Player) const
{
	const FTDNPCRow* Row = GetDefinitionRow();

	const ATDPlayerState* PlayerState =
		Player
			? Player->GetPlayerState<ATDPlayerState>()
			: nullptr;

	const UTDPersonalWorldStateComponent* PersonalState =
		PlayerState
			? PlayerState->GetPersonalWorldStateComponent()
			: nullptr;

	if (Row == nullptr || PersonalState == nullptr)
	{
		return NAME_None;
	}

	for (const FTDNPCDialogueRule& Rule :
		Row->DialogueRules)
	{
		if (!Rule.StartDialogueRow.IsNone()
			&& PersonalState->MatchesCondition(
				Rule.Condition))
		{
			return Rule.StartDialogueRow;
		}
	}

	return NAME_None;
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

	const UTDPersonalWorldStateComponent* PersonalState =
		PlayerState
			? PlayerState->GetPersonalWorldStateComponent()
			: nullptr;

	if (Row == nullptr || PersonalState == nullptr)
	{
		return false;
	}

	if (!PersonalState->MatchesCondition(
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

	ATDPlayerController* PlayerController =
		Cast<ATDPlayerController>(
			Player->GetController());

	if (PlayerController == nullptr)
	{
		return;
	}

	PlayerController->BeginDialogueFromNPC(
		this,
		DialogueTable,
		ResolveDialogueStartRow(Player));
}

FText ATDNPCBase::GetInteractionText_Implementation(
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

void ATDNPCBase::TryBindToLocalPersonalState()
{
	if (GetNetMode() == NM_DedicatedServer
		|| LocalPersonalState != nullptr)
	{
		return;
	}

	APlayerController* LocalController =
		UGameplayStatics::GetPlayerController(
			this,
			0);

	ATDPlayerState* LocalPlayerState =
		LocalController
			? LocalController
				->GetPlayerState<ATDPlayerState>()
			: nullptr;

	if (LocalPlayerState == nullptr)
	{
		return;
	}

	LocalPersonalState =
		LocalPlayerState
			->GetPersonalWorldStateComponent();

	if (LocalPersonalState == nullptr)
	{
		return;
	}

	LocalPersonalState
		->OnPersonalWorldStateChanged
		.AddUniqueDynamic(
			this,
			&ATDNPCBase::HandlePersonalWorldStateChanged);

	GetWorldTimerManager().ClearTimer(
		BindRetryTimerHandle);

	RefreshLocalPresentation();
}

void ATDNPCBase::HandlePersonalWorldStateChanged()
{
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
}

void ATDNPCBase::SetLocalPresentationHidden(
	bool bInHidden)
{
	if (SpriteComponent != nullptr)
	{
		SpriteComponent->SetVisibility(
			!bInHidden,
			true);

		SpriteComponent->SetComponentTickEnabled(
			!bInHidden);
	}

	if (AnimationComponent != nullptr)
	{
		AnimationComponent->SetComponentTickEnabled(
			!bInHidden);
	}

	// 서버 Collision은 다른 플레이어 검증을 위해 유지한다.
	if (!HasAuthority()
		&& InteractionSphere != nullptr)
	{
		InteractionSphere->SetCollisionEnabled(
			bInHidden
				? ECollisionEnabled::NoCollision
				: ECollisionEnabled::QueryOnly);
	}
}