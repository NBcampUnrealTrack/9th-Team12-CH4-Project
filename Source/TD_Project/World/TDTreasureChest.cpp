#include "World/TDTreasureChest.h"

#include "Character/TDPlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Data/TDTreasureChestRow.h"
#include "Items/TDInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PaperFlipbookComponent.h"
#include "Player/TDPlayerController.h"
#include "Player/TDPlayerState.h"
#include "Quest/TDPersonalWorldStateComponent.h"
#include "TimerManager.h"

ATDTreasureChest::ATDTreasureChest()
{
	PrimaryActorTick.bCanEverTick = false;

	// 맵에 고정 배치한다.
	// 위치와 상태를 계속 복제할 필요가 없다.
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

	// 2.5D 스프라이트가 Actor 회전을 따라 종이처럼 돌아가지 않게 한다.
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

	InteractionSphere->SetGenerateOverlapEvents(true);
}

void ATDTreasureChest::BeginPlay()
{
	Super::BeginPlay();

	if (ChestDefinition.DataTable == nullptr
		|| ChestDefinition.RowName.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("상자 '%s': ChestDefinition이 지정되지 않았다."),
			*GetName());
	}
	else if (GetDefinitionRow() == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("상자 '%s': DT_TreasureChest에 Row '%s'가 없다."),
			*GetName(),
			*ChestDefinition.RowName.ToString());
	}

	// 전용 서버에는 화면이 없으므로 로컬 표시 상태를 갱신하지 않는다.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	TryBindToLocalPersonalState();

	if (LocalPersonalState == nullptr)
	{
		// 레벨 Actor의 BeginPlay가 PlayerState 복제보다 먼저 올 수 있다.
		GetWorldTimerManager().SetTimer(
			BindRetryTimerHandle,
			this,
			&ATDTreasureChest::TryBindToLocalPersonalState,
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
		LocalPersonalState->OnPersonalWorldStateChanged.RemoveDynamic(
			this,
			&ATDTreasureChest::HandlePersonalWorldStateChanged);
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

	return ChestDefinition.GetRow<FTDTreasureChestRow>(
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

	if (Row == nullptr
		|| Row->RewardItemId.IsNone()
		|| Row->RewardCount <= 0)
	{
		return false;
	}

	const ATDPlayerState* PlayerState =
		Player->GetPlayerState<ATDPlayerState>();

	const UTDPersonalWorldStateComponent* PersonalState =
		PlayerState
			? PlayerState->GetPersonalWorldStateComponent()
			: nullptr;

	if (PersonalState == nullptr)
	{
		return false;
	}

	const FName ChestId = GetChestId();

	if (ChestId.IsNone()
		|| PersonalState->HasClaimedChest(ChestId))
	{
		return false;
	}

	return PersonalState->MatchesCondition(
		Row->Condition);
}

void ATDTreasureChest::Interact_Implementation(
	ATDPlayerCharacter* Player)
{
	if (!HasAuthority() || !IsValid(Player))
	{
		return;
	}

	ATDPlayerController* PlayerController =
		Cast<ATDPlayerController>(
			Player->GetController());

	ATDPlayerState* PlayerState =
		Player->GetPlayerState<ATDPlayerState>();

	const FTDTreasureChestRow* Row =
		GetDefinitionRow();

	const FName ChestId = GetChestId();

	if (PlayerController == nullptr
		|| PlayerState == nullptr)
	{
		return;
	}

	if (Row == nullptr
		|| ChestId.IsNone()
		|| Row->RewardItemId.IsNone()
		|| Row->RewardCount <= 0)
	{
		PlayerController->ClientInteractionFailed(
			ChestId,
			ETDInteractionFailureReason::InvalidDefinition);

		return;
	}

	UTDPersonalWorldStateComponent* PersonalState =
		PlayerState->GetPersonalWorldStateComponent();

	UTDInventoryComponent* Inventory =
		PlayerState->GetInventoryComponent();

	if (PersonalState == nullptr
		|| Inventory == nullptr)
	{
		PlayerController->ClientInteractionFailed(
			ChestId,
			ETDInteractionFailureReason::InvalidDefinition);

		return;
	}

	// 서버가 실제 실행 직전에 다시 확인한다.
	if (PersonalState->HasClaimedChest(ChestId))
	{
		PlayerController->ClientInteractionFailed(
			ChestId,
			ETDInteractionFailureReason::AlreadyClaimed);

		return;
	}

	if (!PersonalState->MatchesCondition(Row->Condition))
	{
		PlayerController->ClientInteractionFailed(
			ChestId,
			ETDInteractionFailureReason::QuestConditionNotMet);

		return;
	}

	// AddItem은 내부에서 전체 수량이 들어갈 수 있는지 먼저 확인한다.
	// 실패하면 아이템을 일부만 넣지 않으며 상자도 획득 처리하지 않는다.
	if (!Inventory->AddItem(
			Row->RewardItemId,
			Row->RewardCount))
	{
		PlayerController->ClientInteractionFailed(
			ChestId,
			ETDInteractionFailureReason::InventoryFull);

		return;
	}

	// 아이템 지급 성공 후에만 상자를 획득 완료로 기록한다.
	if (!PersonalState->MarkChestClaimed(ChestId))
	{
		// 같은 서버 함수 안에서는 중간에 다른 요청이 끼어들지 않으므로
		// 정상 상황에서는 여기에 도달하지 않는다.
		UE_LOG(LogTemp, Error,
			TEXT("상자 '%s': 아이템 지급 후 획득 상태 기록에 실패했다."),
			*ChestId.ToString());

		return;
	}

	// 이 PlayerController의 소유 클라이언트에만 전달한다.
	PlayerController->ClientChestClaimed(
		ChestId,
		FMath::Max(0.0f, Row->DisappearDelay));

	UE_LOG(LogTemp, Log,
		TEXT("상자 획득: Player='%s', Chest='%s', Item='%s', Count=%d"),
		*PlayerState->GetPlayerName(),
		*ChestId.ToString(),
		*Row->RewardItemId.ToString(),
		Row->RewardCount);
}

FText ATDTreasureChest::GetInteractionText_Implementation(
	ATDPlayerCharacter* Player) const
{
	return NSLOCTEXT(
		"TDInteraction",
		"OpenTreasureChest",
		"상자 열기");
}

void ATDTreasureChest::TryBindToLocalPersonalState()
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
			? LocalController->GetPlayerState<ATDPlayerState>()
			: nullptr;

	if (LocalPlayerState == nullptr)
	{
		return;
	}

	LocalPersonalState =
		LocalPlayerState->GetPersonalWorldStateComponent();

	if (LocalPersonalState == nullptr)
	{
		return;
	}

	LocalPersonalState->OnPersonalWorldStateChanged.AddUniqueDynamic(
		this,
		&ATDTreasureChest::HandlePersonalWorldStateChanged);

	GetWorldTimerManager().ClearTimer(
		BindRetryTimerHandle);

	// 저장 데이터를 이미 받은 상태라면 즉시 맞는 모습으로 바꾼다.
	RefreshLocalPresentation();
}

void ATDTreasureChest::HandlePersonalWorldStateChanged()
{
	RefreshLocalPresentation();
}

void ATDTreasureChest::RefreshLocalPresentation()
{
	if (GetNetMode() == NM_DedicatedServer
		|| LocalPersonalState == nullptr)
	{
		return;
	}

	// 막 획득한 상자는 5초 연출이 끝날 때까지 OnRep 때문에 즉시 숨기지 않는다.
	if (bPlayingClaimPresentation)
	{
		return;
	}

	const bool bClaimed =
		LocalPersonalState->HasClaimedChest(
			GetChestId());

	SetLocalPresentationHidden(bClaimed);
}

void ATDTreasureChest::PlayClaimedPresentation(
	float DisappearDelay)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	bPlayingClaimPresentation = true;

	// OnRep가 먼저 도착해 잠시 숨겨졌더라도 성공 RPC가 오면 다시 보여준다.
	SetLocalPresentationHidden(false);

	// 열린 플립북, 사운드, 이펙트는 BP에서 처리한다.
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
		&ATDTreasureChest::FinishClaimedPresentation,
		SafeDelay,
		false);
}

void ATDTreasureChest::FinishClaimedPresentation()
{
	bPlayingClaimPresentation = false;
	SetLocalPresentationHidden(true);
}

void ATDTreasureChest::SetLocalPresentationHidden(bool bInHidden)
{
	if (SpriteComponent != nullptr)
	{
		SpriteComponent->SetVisibility(
		   !bInHidden,
		   true);

		SpriteComponent->SetComponentTickEnabled(
		   !bInHidden); 
	}

	// 전용 서버의 Collision은 항상 유지한다.
	// 상자를 획득했는지는 서버의 PersonalState가 판정한다.
	//
	// 순수 클라이언트에서는 F 안내 검색에서 제외되도록 로컬 Collision만 끈다.
	if (!HasAuthority()
		&& InteractionSphere != nullptr)
	{
		InteractionSphere->SetCollisionEnabled(
			bInHidden 
				? ECollisionEnabled::NoCollision
				: ECollisionEnabled::QueryOnly);
	}
}