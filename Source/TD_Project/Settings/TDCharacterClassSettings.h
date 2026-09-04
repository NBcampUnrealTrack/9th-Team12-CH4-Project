#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TDCharacterClassSettings.generated.h"

class UDataTable;

/**
 * DT_CharacterClass 를 지정하는 자리. 프로젝트 세팅 > TD > Character 에 나타난다.
 *
 * 경로를 코드에 박지 않기 위한 자리다(D45). UI 는 이 값을 읽어 테이블을 얻고,
 * RowName(=ClassId) 으로 엔진 기본 "Get Data Table Row" 노드를 쓰면 된다.
 *
 *   ATDPlayerState* PS = ...;
 *   FName ClassId = PS->GetCharacterClassId();          // "Warrior" 등
 *   UDataTable* Table = UTDCharacterClassSettings::Get()->ClassTable.LoadSynchronous();
 *   // Get Data Table Row (Table, ClassId) → FTDCharacterClassRow
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "TD Character"))
class TD_PROJECT_API UTDCharacterClassSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("TD"); }

	static const UTDCharacterClassSettings* Get() { return GetDefault<UTDCharacterClassSettings>(); }

	/** DT_CharacterClass. RowName 이 곧 ClassId 다("Warrior" 등). */
	UPROPERTY(config, EditAnywhere, Category = "Character")
	TSoftObjectPtr<UDataTable> ClassTable;
};
