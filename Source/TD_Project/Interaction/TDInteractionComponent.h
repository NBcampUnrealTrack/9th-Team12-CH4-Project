#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TDInteractionComponent.generated.h"

class AActor;

/**
 * 플레이어 주변에서 상호작용 가능한 Actor를 찾아 서버에서 실행하는 컴포넌트.
 *
 * 클라이언트는 F키를 눌렀다는 사실만 서버에 보낸다.
 * 어떤 Actor와 상호작용할지는 서버가 다시 검색해 결정한다.
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDInteractionComponent();

	/** 로컬 플레이어가 F키를 눌렀을 때 호출한다. */
	void RequestInteract();

	/**
	 * 소유 캐릭터 주변에서 상호작용할 수 있는 가장 가까운 Actor를 찾는다.
	 *
	 * 서버에서는 실제 판정에 사용하고,
	 * 클라이언트에서는 나중에 F키 안내 표시에도 사용할 수 있다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Interaction")
	AActor* FindBestInteractable() const;

	/** 현재 상호작용 검색 반경. */
	UFUNCTION(BlueprintPure, Category = "TD|Interaction")
	float GetInteractionRadius() const
	{
		return InteractionRadius;
	}

protected:
	/**
	 * 서버가 직접 주변 Actor를 다시 검색한다.
	 *
	 * Actor를 RPC 인수로 받지 않으므로 클라이언트가 멀리 있는 상자를 위조해
	 * 요청할 수 없다.
	 */
	UFUNCTION(Server, Reliable)
	void ServerRequestInteract();

	/** 플레이어 중심 상호작용 검색 반경. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TD|Interaction",
		meta = (ClampMin = "50.0", UIMin = "50.0"))
	float InteractionRadius = 200.0f;
};