#pragma once

#include "CoreMinimal.h"
#include "Game/TDGameMode.h"
#include "TDUIServerGameMode.generated.h"

/** UI 서버 테스트에서 캐릭터 생성과 선택 과정을 자동으로 처리하는 GameMode. */
UCLASS()
class TD_PROJECT_API ATDUIServerGameMode : public ATDGameMode
{
	GENERATED_BODY()

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/** 자동으로 선택할 테스트 캐릭터 슬롯 번호. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|UI Test", meta = (ClampMin = "0", UIMin = "0"))
	int32 AutoSelectSlotIndex = 0;
};
