#pragma once
#include "CoreMinimal.h"
#include "Game/TDGameMode.h"
#include "TDUI_Login_GameMode.generated.h"

/** 더미 로그인 맵에만 지정하는 UI 테스트 게임 모드. */
UCLASS()
class TD_PROJECT_API ATDUI_Login_GameMode : public ATDGameMode
{
    GENERATED_BODY()
public:
    ATDUI_Login_GameMode();
};
