#pragma once

#include "CoreMinimal.h"
#include "TDNavMenuTypes.generated.h"

UENUM(BlueprintType)
enum class ETDNavMenuType : uint8
{
	Inventory,
	Quest,
	Character,
	Skill,
	Party,
	System
};
