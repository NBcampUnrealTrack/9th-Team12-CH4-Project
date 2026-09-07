#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TDChatSettings.generated.h"

class UDataTable;

/**
 * 채팅 규칙. 프로젝트 세팅 > TD > Chat 에 나타난다.
 *
 * 숫자를 코드에 박지 않는 이유는 운영 중에 바뀔 값들이기 때문이다 — 도배가 심하면
 * 쿨다운을 올리고, 길이 제한은 UI 입력창 크기에 맞춰 조정하게 된다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "TD Chat"))
class TD_PROJECT_API UTDChatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("TD"); }

	static const UTDChatSettings* Get() { return GetDefault<UTDChatSettings>(); }

	/**
	 * DT_BannedWord. 지정하지 않으면 필터가 아무것도 거르지 않는다(D45).
	 *
	 * 서버에서만 읽는다. 클라이언트에 목록을 내려보내지 않는 편이 낫다 —
	 * 무엇을 거르는지 그대로 보이면 우회 목록이 되기 때문이다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Chat")
	TSoftObjectPtr<UDataTable> BannedWordTable;

	/** 한 번에 보낼 수 있는 글자 수. 넘으면 거부한다(잘라내지 않는다). */
	UPROPERTY(config, EditAnywhere, Category = "Chat", meta = (ClampMin = "1"))
	int32 MaxMessageLength = 200;

	/**
	 * 메시지 사이의 최소 간격(초). 도배를 막는다.
	 *
	 * 서버 시각으로만 잰다. 클라이언트가 보낸 시각을 믿으면 그대로 조작된다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Chat", meta = (ClampMin = "0.0"))
	float SendCooldownSeconds = 1.f;
};
