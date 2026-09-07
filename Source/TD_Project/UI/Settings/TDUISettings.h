#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TDUISettings.generated.h"

class UTDTypographyThemeDA;
class UTDWindowBaseWidget;

/** Project Settings > Game > TD UI에 표시되는 프로젝트 공용 UI 설정. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "TD UI"))
class TD_PROJECT_API UTDUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}

	/** 모든 TD Text 위젯이 기본으로 사용할 Typography Theme. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Typography")
	TSoftObjectPtr<UTDTypographyThemeDA> DefaultTypographyTheme;

	/** WindowLayer에 생성할 위젯 클래스. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Windows")
	TSoftClassPtr<UTDWindowBaseWidget> InventoryWindowClass;

	/** Nav의 캐릭터 버튼으로 열 캐릭터 정보 창. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Windows")
	TSoftClassPtr<UTDWindowBaseWidget> CharacterWindowClass;

	/**
	 * 설정 창(그래픽·사운드·화면·단축키).
	 *
	 * Nav의 시스템 버튼과 ESC 메뉴가 같은 창을 연다. 둘을 따로 만들면 한쪽만
	 * 고쳤을 때 화면이 갈라진다.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Windows")
	TSoftClassPtr<UTDWindowBaseWidget> SystemWindowClass;
};
