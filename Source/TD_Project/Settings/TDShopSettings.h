#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TDShopSettings.generated.h"

class UDataTable;

/**
 * NPC 상점 규칙. 프로젝트 세팅 > TD > Shop 에 나타난다.
 *
 * 거래소(UTDMarketSettings)와 다른 시스템이다 — 이쪽은 플레이어 간이 아니라
 * NPC 가 고정 목록을 판다. 재고·가격이 전부 데이터라 서버가 기억할 것이 없고,
 * 따라서 저장할 것도 없다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "TD Shop"))
class TD_PROJECT_API UTDShopSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("TD"); }

	static const UTDShopSettings* Get() { return GetDefault<UTDShopSettings>(); }

	/** DT_Shop. 상점 정의. */
	UPROPERTY(config, EditAnywhere, Category = "Shop")
	TSoftObjectPtr<UDataTable> ShopTable;

	/** DT_ShopItem. 무엇을 얼마에 파는가. 되사는 값도 여기서 나온다. */
	UPROPERTY(config, EditAnywhere, Category = "Shop")
	TSoftObjectPtr<UDataTable> ShopItemTable;

	/**
	 * 되팔 때 구매가의 몇 배를 쳐주는가. 0.25 면 100원짜리를 25원에 산다.
	 *
	 * DT_Shop 의 SellBackRateOverride 가 0 보다 크면 그쪽이 이긴다.
	 *
	 * 1.0 을 넘기면 안 된다. 사고 되파는 것만으로 돈이 늘어난다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Shop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SellBackRate = 0.25f;

	/**
	 * 상인에게서 이 거리 안에 있어야 거래할 수 있다(cm).
	 *
	 * 서버가 검증한다. 없으면 어디서든 상점을 쓸 수 있다 — 마을에 돌아갈 이유가 사라진다.
	 * NPC 의 대화 거리(기본 350)보다 넉넉하게 잡아, 대화 중에 창을 열었다가
	 * 살짝 움직였다고 거래가 거부되는 일을 막는다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Shop", meta = (ClampMin = "0"))
	float InteractRange = 500.f;

	/**
	 * 한 번에 살 수 있는 최대 개수.
	 *
	 * 상한이 없으면 클라이언트가 20억 개를 요청해 가격 계산이 넘친다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Shop", meta = (ClampMin = "1"))
	int32 MaxCountPerTrade = 999;
};
