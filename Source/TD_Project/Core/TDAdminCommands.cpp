#include "Chat/TDChatTypes.h"
#include "Engine/World.h"
#include "Game/TDGameMode.h"
#include "HAL/IConsoleManager.h"

/**
 * 운영 명령.
 *
 * TDDebugCommands 와 나눠 둔 이유는 **배포 빌드에 남기 때문**이다. 그쪽 치트들은
 * 구현부가 UE_BUILD_SHIPPING 으로 막혀 있어 출시 빌드에서는 호출해도 아무 일이
 * 일어나지 않지만, 공지는 운영 중에 실제로 써야 하는 기능이라 살아 있어야 한다.
 * 한 파일에 섞어 두면 나중에 치트를 정리하다 이것까지 함께 지우게 된다.
 *
 * 이름을 TD.* 가 아니라 TDAdmin.* 으로 둔 것도 같은 이유다 — 출시 빌드에 남는
 * 명령과 그렇지 않은 명령을 이름만 보고 구분할 수 있다.
 *
 * ── 안전장치 ──
 * 콘솔 명령은 로컬에서 실행되므로 클라이언트도 입력 자체는 할 수 있다. 다만
 * GetAuthGameMode 가 서버에서만 유효해서, 클라이언트가 쳐도 아무 일이 일어나지
 * 않는다. 전용 서버의 콘솔이나 리슨 서버 호스트에서만 실제로 나간다.
 */
DEFINE_LOG_CATEGORY_STATIC(LogTDAdmin, Log, All);

namespace TDAdminCommands
{
	/** 접속자 전원에게 공지를 띄운다. */
	static void Notice(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() == 0)
		{
			UE_LOG(LogTDAdmin, Warning,
				TEXT("사용법: TDAdmin.Notice <공지 내용>   예) TDAdmin.Notice 5분 뒤 점검을 시작합니다"));
			return;
		}

		ATDGameMode* GameMode = World ? World->GetAuthGameMode<ATDGameMode>() : nullptr;
		if (GameMode == nullptr)
		{
			// 클라이언트에서 친 경우가 대부분이다. 조용히 무시하지 않고 이유를 남긴다.
			UE_LOG(LogTDAdmin, Warning,
				TEXT("TDAdmin.Notice 는 서버에서만 동작한다. (전용 서버 콘솔 또는 리슨 서버 호스트)"));
			return;
		}

		// 콘솔이 공백마다 인자를 잘라 주므로 한 문장으로 되돌린다.
		FString Message;
		for (const FString& Arg : Args)
		{
			if (!Message.IsEmpty())
			{
				Message.AppendChar(TEXT(' '));
			}
			Message += Arg;
		}

		GameMode->BroadcastSystemMessage(Message);

		UE_LOG(LogTDAdmin, Log, TEXT("공지를 보냈다: %s"), *Message);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GTDAdminNotice(
	TEXT("TDAdmin.Notice"),
	TEXT("접속자 전원에게 공지를 보낸다(서버 전용). 사용법: TDAdmin.Notice <공지 내용>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDAdminCommands::Notice));
