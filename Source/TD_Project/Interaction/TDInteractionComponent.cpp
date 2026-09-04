#include "Interaction/TDInteractionComponent.h"

#include "Character/TDPlayerCharacter.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interaction/TDInteractable.h"

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
	// 클라이언트에서 호출하면 서버 RPC,
	// 서버에서 직접 호출하면 즉시 서버 구현부가 실행된다.
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

	// 서버가 자기 월드에서 직접 가장 가까운 대상을 찾는다.
	AActor* Target = FindBestInteractable();

	if (!IsValid(Target)
		|| !Target->Implements<UTDInteractable>())
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Verbose,
			TEXT("%s: 상호작용 범위 안에 사용할 수 있는 대상이 없다."),
			*GetNameSafe(Player));
#endif
		return;
	}

	// FindBestInteractable에서도 검사했지만 실제 실행 직전에 다시 검사한다.
	// 앞선 검사 직후 다른 요청이 상자를 획득하는 등의 상태 변화가 생길 수 있다.
	if (!ITDInteractable::Execute_CanInteract(Target, Player))
	{
		return;
	}

	ITDInteractable::Execute_Interact(Target, Player);
}