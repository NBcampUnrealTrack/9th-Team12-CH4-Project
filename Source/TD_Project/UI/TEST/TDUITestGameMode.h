#pragma once

#include "CoreMinimal.h"
#include "Game/TDGameMode.h"
#include "TDUITestGameMode.generated.h"

/** 인벤토리 UI 확인용 PlayerController를 기본으로 사용하는 임시 GameMode. */
UCLASS()
class TD_PROJECT_API ATDUITestGameMode : public ATDGameMode
{
	GENERATED_BODY()

public:
	ATDUITestGameMode();
};
