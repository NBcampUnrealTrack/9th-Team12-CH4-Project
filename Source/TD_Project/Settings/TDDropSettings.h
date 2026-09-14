#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TDDropSettings.generated.h"

class UDataTable;

/**
 * 몬스터 드롭 규칙. 프로젝트 세팅 > TD > Drop 에 나타난다.
 *
 * 테이블을 몬스터 BP 가 아니라 여기 둔다. 드롭 테이블은 몬스터마다 다를 이유가 없고 —
 * 어느 줄을 볼지는 이미 DT_MonsterDefinition.DropTableId 가 정한다 — BP 마다 지정하게 하면
 * 하나를 빠뜨렸을 때 그 몬스터만 조용히 아무것도 떨구지 않는다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "TD Drop"))
class TD_PROJECT_API UTDDropSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("TD"); }

	static const UTDDropSettings* Get() { return GetDefault<UTDDropSettings>(); }

	/** DT_DropTable. 비어 있으면 어느 몬스터도 아이템을 떨구지 않는다(경험치·골드는 그대로 나간다). */
	UPROPERTY(config, EditAnywhere, Category = "Drop")
	TSoftObjectPtr<UDataTable> DropTable;
};
