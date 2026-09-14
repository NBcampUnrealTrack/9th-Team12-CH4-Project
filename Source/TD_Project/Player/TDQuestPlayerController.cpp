#include "Player/TDQuestPlayerController.h"

#include "Interaction/TDInteractionFlowComponent.h"
#include "Shop/TDShopServiceComponent.h"

ATDQuestPlayerController::
ATDQuestPlayerController()
{
	InteractionFlowComponent =
		CreateDefaultSubobject<
			UTDInteractionFlowComponent>(
				TEXT("InteractionFlowComponent"));

	ShopServiceComponent =
		CreateDefaultSubobject<UTDShopServiceComponent>(
			TEXT("ShopServiceComponent"));
}
