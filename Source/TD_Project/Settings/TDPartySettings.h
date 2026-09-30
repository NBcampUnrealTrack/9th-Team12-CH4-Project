#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TDPartySettings.generated.h"

/**
 * 파티 규칙. 프로젝트 세팅 > TD > Party 에 나타난다.
 *
 * 밸런스 값이라 코드에 박지 않는다. 인원별 경험치 배율은 실제로 플레이해 보고
 * 조정하게 되는 종류의 숫자다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "TD Party"))
class TD_PROJECT_API UTDPartySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("TD"); }

	/** 파티 최대 인원. 던전 난이도가 이 값을 기준으로 설계되므로 함부로 바꾸지 않는다. */
	UPROPERTY(config, EditAnywhere, Category = "Party", meta = (ClampMin = "2", ClampMax = "8"))
	int32 MaxPartySize = 4;

	/**
	 * 인원수별 경험치 가산율. 배열 인덱스가 **인원수 - 1** 이다.
	 *
	 *   [0] 혼자      0.0   (보너스 없음)
	 *   [1] 2명       0.1   (+10%)
	 *   [2] 3명       0.2
	 *   [3] 4명       0.3
	 *
	 * 인원수가 배열 길이를 넘으면 마지막 값을 쓴다. MaxPartySize 를 늘려도
	 * 배열을 안 늘렸다고 경험치가 0 이 되지는 않는다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Party")
	TArray<float> ExpBonusByMemberCount = { 0.f, 0.1f, 0.2f, 0.3f };

	/**
	 * 초대장이 만료되기까지의 시간(초).
	 *
	 * 만료가 없으면 오래전 초대가 남아 있다가 엉뚱한 때에 수락되고,
	 * 서버 메모리에도 계속 쌓인다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Party", meta = (ClampMin = "5.0"))
	float InviteExpireSeconds = 60.f;

	/**
	 * 초대를 받을 수 있는 최대 거리(cm). 0 이면 거리 제한 없음.
	 *
	 * 제한을 두면 모르는 사람에게서 무작위 초대가 오는 것을 막을 수 있다.
	 * 다만 파티 모집 UI 가 없어서(D72) 너무 좁으면 파티를 맺기 어려워진다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Party", meta = (ClampMin = "0.0"))
	float MaxInviteDistance = 0.f;
};
