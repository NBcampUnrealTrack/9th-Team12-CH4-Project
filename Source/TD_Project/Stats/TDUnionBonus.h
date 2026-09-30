#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Save/TDPlayerSaveData.h"
#include "Stats/TDStatTypes.h"

class UDataTable;

/** 유니온 표의 한 줄과, 지금 계정이 그 줄을 켰는지. 유니온 창이 그리는 단위다. */
struct FTDUnionEntry
{
	FName ClassId;
	int32 RequiredLevel = 0;
	FGameplayTag StatTag;
	ETDModOp Op = ETDModOp::Added;
	float Value = 0.f;
	bool bActive = false;
};

/**
 * 유니온 보너스 계산. 액터도 월드도 필요 없는 순수 함수로 둔다(D2 와 같은 이유).
 *
 * **규칙을 여기 한 곳에 모은다.** 서버(ATDPlayerState — 스탯 등록)와 화면(유니온 창 — 색 표시)이
 * 같은 함수를 쓴다. 따로 계산하면 창에는 켜졌다고 나오는데 스탯은 안 오른 상태가 생긴다.
 *
 * 공통 인자
 *   Characters    계정의 캐릭터 요약. 레벨은 **저장된 값**이다.
 *   CurrentSlot   지금 플레이 중인 슬롯. 이 슬롯은 CurrentLevel 로 대신 센다 —
 *                 저장 전에 오른 레벨이 곧바로 반영되어야 하기 때문이다.
 *                 INDEX_NONE 이면 목록 값을 그대로 쓴다.
 *   CurrentLevel  지금 플레이 중인 캐릭터의 실시간 레벨.
 */
namespace TDUnion
{
	/**
	 * 직업마다 가장 높은 레벨. 같은 직업을 여럿 키워도 한 번만 센다 —
	 * 그래야 보상의 최대치가 "세 직업 × 만렙" 으로 고정되고, 한 직업만 여러 번 키우는 쪽이
	 * 이득이 되지 않는다.
	 */
	TMap<FName, int32> GetBestLevels(TConstArrayView<FTDCharacterSummary> Characters,
		int32 CurrentSlot, int32 CurrentLevel);

	/** 표의 모든 줄과 켜짐 여부. 테이블 행 순서 그대로다. */
	TArray<FTDUnionEntry> BuildEntries(const UDataTable* BonusTable,
		TConstArrayView<FTDCharacterSummary> Characters, int32 CurrentSlot, int32 CurrentLevel);

	/**
	 * 켜진 줄만 모디파이어로 만든다. 서버가 스탯에 등록할 때 쓴다.
	 * @return 30레벨에 닿은 직업이 없으면 비어 있다.
	 */
	TArray<FTDStatModifier> BuildModifiers(const UDataTable* BonusTable,
		TConstArrayView<FTDCharacterSummary> Characters, int32 CurrentSlot, int32 CurrentLevel);
}
