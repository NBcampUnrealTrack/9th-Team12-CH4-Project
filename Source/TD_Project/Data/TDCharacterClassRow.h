#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TDCharacterClassRow.generated.h"

class UTDCharacterClassData;

/**
 * DT_CharacterClass 의 행. 직업 하나의 **표시 정보**다.
 *
 * 성장(스탯이 얼마나 오르는가)은 DT_ClassGrowth 가 담당하고, 이 테이블은
 * 화면에 보여줄 것만 담는다 — 계산 로직과 표시 데이터가 같은 테이블에 있으면
 * 기획이 이름 하나 바꾸려고 성장 수치까지 있는 테이블을 열어야 한다.
 *
 * 식별자는 **RowName** 이다. 별도 ClassId 필드를 두지 않는 이유는 DT_ItemDefinition 과
 * 같다 — 필드를 따로 두면 같은 값이 두 곳에 존재하게 된다. RowName 은
 * DT_ClassGrowth.ClassId · FTDCharacterSummary.ClassId · ATDPlayerState.CharacterClassId 와
 * 정확히 같은 문자열이어야 한다("Warrior" / "Mage" / "Archer").
 *
 * 같은 이유로 RowName 기준 조회이므로, 언리얼 기본 제공 Blueprint 노드인
 * "Get Data Table Row" 를 그대로 쓸 수 있다 — 이 테이블만을 위한 C++ 조회 함수를
 * 따로 만들지 않았다.
 */
USTRUCT(BlueprintType)
struct FTDCharacterClassRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 화면에 보이는 직업명. 지금 코드의 "Warrior" 같은 영문 식별자와는 다른 값이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class")
	FText DisplayName;

	/**
	 * 이 직업의 그림 묶음(아이콘·얼굴·전신). **CSV 로 채우지 말고 에디터에서 물릴 것** —
	 * 경로를 손으로 적으면 오타가 임포트 때 조용히 빈 참조가 된다.
	 *
	 * 텍스처를 이 행에 직접 두지 않은 이유는 전신 그림 때문이다. UTDCharacterClassData
	 * 주석에 적어 두었다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class")
	TSoftObjectPtr<UTDCharacterClassData> VisualData;
};
