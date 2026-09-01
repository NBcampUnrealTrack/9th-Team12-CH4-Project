#include "TDUIServerGameMode.h"

#include "Player/TDPlayerController.h"
#include "Player/TDPlayerState.h"

void ATDUIServerGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
#if !UE_BUILD_SHIPPING
	ATDPlayerController* TDPlayerController = Cast<ATDPlayerController>(NewPlayer);
	ATDPlayerState* TDPlayerState = NewPlayer ? NewPlayer->GetPlayerState<ATDPlayerState>() : nullptr;

	if (TDPlayerController && TDPlayerState && !TDPlayerState->HasSelectedCharacter())
	{
		// TD.GiveTestCharacters와 같은 서버 경로를 직접 호출한다.
		TDPlayerController->ServerDebugGiveTestCharacters();

		// 성공하면 ATDGameMode::HandleCharacterSelected가 Pawn까지 스폰한다.
		if (TDPlayerState->SelectCharacter(AutoSelectSlotIndex))
		{
			UE_LOG(LogTemp, Log, TEXT("%s — UI 서버 테스트 캐릭터 %d번 자동 선택 완료."),
				*GetNameSafe(NewPlayer), AutoSelectSlotIndex);
			return;
		}
	}
#endif

	// 자동 선택 실패 또는 Shipping 빌드에서는 기존 캐릭터 선택 흐름을 사용한다.
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}
