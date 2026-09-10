#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TDItemOptionSettings.generated.h"

class UDataTable;

/**
 * 추가 옵션(잠재능력) 규칙. 프로젝트 세팅 > TD > Item Option 에 나타난다.
 *
 * 등급별 확률·비용은 DT_OptionRarity 에 있다. 여기 있는 것은 등급과 무관하게
 * 전체에 걸리는 값들이다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "TD Item Option"))
class TD_PROJECT_API UTDItemOptionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("TD"); }

	static const UTDItemOptionSettings* Get() { return GetDefault<UTDItemOptionSettings>(); }

	/** DT_OptionRarity. 등급 순서와 등급별 확률·비용. */
	UPROPERTY(config, EditAnywhere, Category = "Item Option")
	TSoftObjectPtr<UDataTable> OptionRarityTable;

	/** DT_OptionPool. (풀, 등급) 조합에서 어떤 옵션이 뽑히는지. */
	UPROPERTY(config, EditAnywhere, Category = "Item Option")
	TSoftObjectPtr<UDataTable> OptionPoolTable;

	/**
	 * 한 아이템에 붙는 옵션 줄 수. 등급과 무관하게 같다.
	 *
	 * 등급 차이는 줄 수가 아니라 **어느 풀에서 뽑히는가**로 나타난다.
	 * 줄 수까지 다르면 UI 레이아웃이 등급마다 달라지고, 낮은 등급 장비의 툴팁에
	 * 빈 칸이 남는다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Item Option", meta = (ClampMin = "1", ClampMax = "6"))
	int32 OptionLineCount = 3;

	/**
	 * 착용 레벨제한이 최고인 아이템의 재굴림 비용 배율.
	 *
	 * 레벨 0 짜리는 1.0 배, 만렙 제한 장비는 이 값만큼 든다. 그 사이는 선형이다 —
	 * 구간(20·40·50)으로 끊지 않은 이유는 중간 레벨 장비가 생겼을 때 빈 구간이
	 * 없어야 하기 때문이다. 35레벨 장비가 추가돼도 값이 자동으로 나온다.
	 *
	 * 강화가 스탯 상승폭을 착용 레벨로 보간하는 것과 같은 방식이다(TDEnhance).
	 */
	UPROPERTY(config, EditAnywhere, Category = "Item Option", meta = (ClampMin = "1.0"))
	float MaxLevelCostMultiplier = 5.f;
};
