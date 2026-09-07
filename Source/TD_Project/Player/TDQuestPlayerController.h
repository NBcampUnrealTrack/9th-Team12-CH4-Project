#pragma once

#include "CoreMinimal.h"
#include "Player/TDPlayerController.h"
#include "TDQuestPlayerController.generated.h"

class UTDInteractionFlowComponent;

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

private:
	UPROPERTY(VisibleAnywhere,
		Category = "TD|Interaction")
	TObjectPtr<UTDInteractionFlowComponent>
		InteractionFlowComponent;
};