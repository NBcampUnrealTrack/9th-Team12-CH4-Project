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

	/**
	 * DT_UnionBonus. 직업을 몇 레벨까지 키우면 계정의 모든 캐릭터가 무엇을 얻는가.
	 *
	 * 비어 있으면 유니온 보너스가 붙지 않는다. 오류 없이 조용히 빠지므로 검증 스크립트가 잡는다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Character")
	TSoftObjectPtr<UDataTable> UnionBonusTable;
};
