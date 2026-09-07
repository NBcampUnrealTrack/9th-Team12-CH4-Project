#include "Player/TDQuestPlayerController.h"

#include "Interaction/TDInteractionFlowComponent.h"

ATDQuestPlayerController::
ATDQuestPlayerController()
{
	InteractionFlowComponent =
		CreateDefaultSubobject<
			UTDInteractionFlowComponent>(
				TEXT("InteractionFlowComponent"));
}