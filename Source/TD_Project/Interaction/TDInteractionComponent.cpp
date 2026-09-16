#include "Interaction/TDInteractionComponent.h"

#include "Character/TDPlayerCharacter.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interaction/TDInteractable.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TDInteractionFlowComponent.h"

UTDInteractionComponent::UTDInteractionComponent()
{
	// F키를 누를 때만 검색하므로 매 프레임 Tick은 필요 없다.
	PrimaryComponentTick.bCanEverTick = false;

	// ActorComponent의 Server RPC가 정상적으로 전달되려면
	// 소유 Actor와 함께 복제되는 컴포넌트여야 한다.
	SetIsReplicatedByDefault(true);
}

void UTDInteractionComponent::RequestInteract()
{
	ATDPlayerCharacter* Player = Cast<ATDPlayerCharacter>(GetOwner());
	if (Player == nullptr || Player->IsDead()) return;
	APlayerController* OwningController = Cast<APlayerController>(Player->GetController());
	UTDInteractionFlowComponent* Flow = OwningController ? OwningController->FindComponentByClass<UTDInteractionFlowComponent>() : nullptr;
	// 1) 챕터 연출 중이면 F키 상호작용 무시
	if (Flow != nullptr && Flow->IsChapterPresentationActive())
	{
		return;
	}
	// 2) 대화 중이면 대화 넘기기/수락 처리
	if (Flow != nullptr && Flow->TryHandleDialogueInteractInput())
	{
		return;
	}
	ServerRequestInteract();
}

AActor* UTDInteractionComponent::FindBestInteractable() const
{
	ATDPlayerCharacter* Player =
		Cast<ATDPlayerCharacter>(GetOwner());

	if (Player == nullptr || Player->IsDead())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	// WorldDynamic:
	// NPC와 상자의 InteractionSphere가 기본적으로 사용하는 오브젝트 타입.
	//
	// WorldStatic:
	// 레벨에 고정 배치된 상호작용 Actor가 Static 타입으로 설정된 경우도 검색한다.
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	// 자기 캐릭터는 검색 결과에서 제외한다.
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(TDInteractionOverlap),
		false,
		Player);

	TArray<FOverlapResult> OverlapResults;

	const bool bFoundAny = World->OverlapMultiByObjectType(
		OverlapResults,
		Player->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(InteractionRadius),
		QueryParams);

	if (!bFoundAny)
	{
		return nullptr;
	}

	AActor* BestActor = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	// 한 Actor에 여러 Collision Component가 있으면 Overlap 결과도 여러 개가
	// 들어올 수 있으므로 이미 확인한 Actor를 중복 검사하지 않는다.
	TSet<AActor*> CheckedActors;

	for (const FOverlapResult& Result : OverlapResults)
	{
		AActor* Candidate = Result.GetActor();

		if (!IsValid(Candidate)
			|| Candidate == Player
			|| CheckedActors.Contains(Candidate))
		{
			continue;
		}

		CheckedActors.Add(Candidate);

		if (!Candidate->Implements<UTDInteractable>())
		{
			continue;
		}

		// 이미 획득한 상자, 퀘스트 조건이 맞지 않는 NPC 등은 후보에서 제외한다.
		if (!ITDInteractable::Execute_CanInteract(Candidate, Player))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			Player->GetActorLocation(),
			Candidate->GetActorLocation());

		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestActor = Candidate;
		}
	}

	return BestActor;
}

void UTDInteractionComponent::ServerRequestInteract_Implementation()
{
	ATDPlayerCharacter* Player =
		Cast<ATDPlayerCharacter>(GetOwner());

	if (Player == nullptr || Player->IsDead())
	{
		return;
	}

	APlayerController* OwningController =
		Cast<APlayerController>(Player->GetController());

	const UTDInteractionFlowComponent* Flow =
		OwningController
			? OwningController->FindComponentByClass<
				UTDInteractionFlowComponent>()
			: nullptr;

	// 서버에 이미 대화가 열려 있으면 일반 상호작용을 추가 실행하지 않는다.
	//
	// 대화 화면이 클라이언트에 도착하기 전에 F를 다시 누르는 등,
	// 네트워크 지연 중 들어온 일반 상호작용 요청도 여기에서 막는다.
	//
	// 다음/수락은 별도의 기존 대화 서버 함수가 담당한다.
	if (Flow != nullptr && Flow->IsDialogueActive())
	{
		return;
	}

	// 기존 방식대로 서버가 주변 상호작용 대상을 직접 찾는다.
	AActor* Target = FindBestInteractable();

	if (!IsValid(Target)
		|| !Target->Implements<UTDInteractable>())
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(
			LogTemp,
			Verbose,
			TEXT("%s: 상호작용 범위 안에 사용할 수 있는 대상이 없다."),
			*GetNameSafe(Player));
#endif
		return;
	}

	// 실제 실행 직전에 다시 상호작용 가능 여부를 검사한다.
	if (!ITDInteractable::Execute_CanInteract(Target, Player))
	{
		return;
	}

	ITDInteractable::Execute_Interact(Target, Player);
}