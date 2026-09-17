#include "Core/TDCheatAccess.h"

#include "HAL/PlatformMisc.h"

namespace TDCheatAccess
{
	namespace
	{
		/** 이 프로세스에서 치트가 열렸는가. 저장하지 않는다 — 껐다 켜면 다시 잠긴다. */
		bool bUnlocked = false;
	}

	bool IsUnlockedLocally()
	{
#if WITH_EDITOR
		// 개발 중에는 늘 열어 둔다. 매번 암호를 치게 만들 이유가 없고,
		// 이 분기는 패키지 빌드에서 컴파일되지 않는다.
		return true;
#else
		return bUnlocked;
#endif
	}

	void SetUnlockedLocally(bool bInUnlocked)
	{
		bUnlocked = bInUnlocked;
	}

	bool VerifyKey(const FString& Key)
	{
		const FString Expected = FPlatformMisc::GetEnvironmentVariable(TEXT("TD_CHEAT_KEY"));

		// 환경변수를 설정하지 않은 서버는 잠긴 채로 둔다. 빈 암호로 열리면
		// "설정을 깜빡한 서버" 가 곧 "치트가 열린 서버" 가 된다.
		if (Expected.IsEmpty() || Key.IsEmpty())
		{
			return false;
		}

		return Key.Equals(Expected, ESearchCase::CaseSensitive);
	}
}
