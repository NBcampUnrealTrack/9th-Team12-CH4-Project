#pragma once

#include "CoreMinimal.h"
#include "TDChatTypes.generated.h"

/**
 * 채팅 채널.
 *
 * 식별자를 태그로 두는 규칙(D9)에서 벗어난 자리다. 태그가 필요한 것은 계층 쿼리로
 * 광역 처리를 하는 데이터 식별자인데, 채널은 **채널마다 받을 사람을 추리는 코드가
 * 전부 다르다** — 새 채널이 생기면 반드시 라우팅 코드를 손봐야 하므로 데이터로
 * 늘어나는 종류가 아니다. enum 이면 switch 에서 빠뜨린 case 를 컴파일러가 잡아준다.
 * ETDQuickSlotType 과 같은 부류다.
 */
UENUM(BlueprintType)
enum class ETDChatChannel : uint8
{
	/** 접속자 전원. */
	All			UMETA(DisplayName = "전체"),

	/** 같은 PartyId 인 사람에게만. */
	Party		UMETA(DisplayName = "파티"),

	/** 지정한 한 명에게만. 보낸 사람도 자기 화면에서 확인할 수 있게 함께 받는다. */
	Whisper		UMETA(DisplayName = "귓속말"),

	/**
	 * 서버가 보내는 공지·경고. **클라이언트는 이 채널로 보낼 수 없다** —
	 * 막지 않으면 누구나 "[시스템] 점검이 시작됩니다" 를 찍을 수 있다.
	 *
	 * UI 는 어느 탭에서든 보여준다.
	 */
	System		UMETA(DisplayName = "시스템"),

	/**
	 * 아이템·경험치·골드 획득 알림. 서버 전용이고 **본인에게만** 간다.
	 *
	 * System 과 나눈 이유는 양 때문이다. 획득 알림은 초당 여러 줄이 올라와서
	 * 같은 칸에 섞으면 정작 읽어야 할 대화가 밀려 올라간다. 채널을 나눠 두면
	 * UI 가 전용 탭이나 별도 위젯으로 빼서 대화와 분리할 수 있다.
	 */
	Loot		UMETA(DisplayName = "획득")
};

/**
 * 채팅 전송 요청의 결과. 거부됐을 때 보낸 사람에게만 돌려준다.
 *
 * 문구가 아니라 enum 인 이유는 ETDZoneTravelResult 와 같다 — 서버가 완성된 문장을
 * 보내면 현지화도 못 하고 화면 디자인이 서버 코드에 묶인다.
 */
UENUM(BlueprintType)
enum class ETDChatSendResult : uint8
{
	Success				UMETA(DisplayName = "성공"),

	/** 내용이 비었거나 공백뿐이다. */
	Empty				UMETA(DisplayName = "내용 없음"),

	TooLong				UMETA(DisplayName = "너무 김"),

	/** 도배 방지 쿨다운에 걸렸다. */
	TooFast				UMETA(DisplayName = "너무 빠름"),

	/** System·Loot 처럼 클라이언트가 보낼 수 없는 채널이다. */
	ChannelNotAllowed	UMETA(DisplayName = "보낼 수 없는 채널"),

	/** 파티 채팅인데 파티에 속해 있지 않다. */
	NotInParty			UMETA(DisplayName = "파티 없음"),

	/** 귓속말 대상을 찾지 못했다. 접속 중이 아니거나 이름이 틀렸다. */
	TargetNotFound		UMETA(DisplayName = "대상 없음")
};
