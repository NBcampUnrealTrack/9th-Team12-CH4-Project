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
	System,

	// 뒤에 붙인다. 중간에 넣으면 블루프린트에 저장된 기존 값의 번호가 밀린다.
	Union,
	Market
};
