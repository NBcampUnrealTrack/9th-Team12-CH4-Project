#pragma once

#include "CoreMinimal.h"
#include "Player/TDPlayerController.h"
#include "TDQuestPlayerController.generated.h"

class UTDInteractionFlowComponent;
class UTDShopServiceComponent;

UCLASS()
class TD_PROJECT_API ATDQuestPlayerController
	: public ATDPlayerController
{
	GENERATED_BODY()

public:
	ATDQuestPlayerController();

	UFUNCTION(BlueprintPure,
		Category = "TD|Interaction")
	UTDInteractionFlowComponent*
	GetInteractionFlowComponent() const
	{
		return InteractionFlowComponent;
	}

	UFUNCTION(BlueprintPure, Category = "TD|Shop")
	UTDShopServiceComponent* GetShopServiceComponent() const
	{
		return ShopServiceComponent;
	}

private:
	UPROPERTY(VisibleAnywhere,
		Category = "TD|Interaction")
	TObjectPtr<UTDInteractionFlowComponent>
		InteractionFlowComponent;

	/** NPC 대화가 끝난 뒤에만 열리는 서버 권위 상점 세션. */
	UPROPERTY(VisibleAnywhere, Category = "TD|Shop")
	TObjectPtr<UTDShopServiceComponent> ShopServiceComponent;
};
