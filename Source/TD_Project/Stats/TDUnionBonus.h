#pragma once

#include "CoreMinimal.h"
#include "Save/TDPlayerSaveData.h"
#include "Stats/TDStatTypes.h"

class UDataTable;

/**
 * 유니온 보너스 계산. 액터도 월드도 필요 없는 순수 함수로 둔다(D2 와 같은 이유).
 *
 * 규칙을 여기 한 곳에 모은다. 지금은 PlayerState 만 부르지만, 유니온 창이 "다음 단계까지"
 * 를 보여주게 되면 같은 함수를 써야 한다 — 두 곳에서 따로 계산하면 화면과 실제 적용값이 갈린다.
 */
namespace TDUnion
{
	/**
	 * 계정의 캐릭터 목록으로 유니온 모디파이어를 만든다.
	 *
	 * 직업마다 가장 높은 레벨 하나를 보고, 그 레벨에 도달한 행을 전부 모은다(누적).
	 *
	 * @param Characters    계정의 캐릭터 요약. 레벨은 **저장된 값**이다.
	 * @param CurrentSlot   지금 플레이 중인 슬롯. 이 슬롯은 CurrentLevel 로 대신 센다 —
	 *                      저장 전에 오른 레벨이 곧바로 반영되어야 하기 때문이다.
	 *                      INDEX_NONE 이면 목록 값을 그대로 쓴다.
	 * @param CurrentLevel  지금 플레이 중인 캐릭터의 실시간 레벨.
	 * @return 켜진 효과들. 30레벨에 닿은 직업이 없으면 비어 있다.
	 */
	TArray<FTDStatModifier> BuildModifiers(const UDataTable* BonusTable,
		TConstArrayView<FTDCharacterSummary> Characters, int32 CurrentSlot, int32 CurrentLevel);
}
