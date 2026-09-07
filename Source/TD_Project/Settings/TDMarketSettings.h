#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TDMarketSettings.generated.h"

/**
 * 거래소 규칙. 프로젝트 세팅 > TD > Market 에 나타난다.
 *
 * 전부 운영 중에 조정하게 될 값이라 코드에 박지 않는다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "TD Market"))
class TD_PROJECT_API UTDMarketSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("TD"); }

	static const UTDMarketSettings* Get() { return GetDefault<UTDMarketSettings>(); }

	/**
	 * 판매 수수료. 0.05 면 5% 다.
	 *
	 * **팔릴 때 판매자 대금에서 뗀다.** 등록할 때 떼면 안 팔린 물건을 취소할 때
	 * 돌려줄지가 또 결정거리가 되는데, 돌려주면 수수료 의미가 없고 안 돌려주면
	 * 등록 자체를 꺼리게 된다.
	 *
	 * 떼인 만큼은 사라진다. 이것이 이 수수료의 목적이다 — 골드가 쌓이기만 하면
	 * 물가가 오르므로 빠져나가는 구멍이 있어야 한다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Market", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SaleFeeRate = 0.05f;

	/**
	 * 한 사람이 동시에 올릴 수 있는 매물 수.
	 *
	 * 상한이 없으면 인벤토리를 통째로 올려 거래소를 창고처럼 쓰게 된다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Market", meta = (ClampMin = "1"))
	int32 MaxListingsPerPlayer = 10;

	/** 매길 수 있는 최고 가격. 골드 상한(20억)보다 낮게 둬 계산 중 넘치지 않게 한다. */
	UPROPERTY(config, EditAnywhere, Category = "Market", meta = (ClampMin = "1"))
	int32 MaxPrice = 100000000;

	/**
	 * 검색 한 번에 돌려주는 최대 개수.
	 *
	 * 전부 보내면 RPC 하나가 지나치게 커진다. UI 는 페이지를 넘겨 더 받는다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Market", meta = (ClampMin = "1", ClampMax = "200"))
	int32 SearchPageSize = 50;
};
