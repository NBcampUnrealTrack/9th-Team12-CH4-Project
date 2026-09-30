#pragma once

#include "CoreMinimal.h"
#include "Data/TDOptionRarityRow.h"
#include "Data/TDOptionRow.h"
#include "GameplayTagContainer.h"
#include "Items/TDItemTypes.h"

/**
 * 추가 옵션(잠재능력) 계산.
 *
 * 강화(TDEnhance)와 같은 구조다 — 액터도 월드도 컴포넌트도 필요 없는 순수 함수로 두고,
 * **주사위 값을 밖에서 넣어준다.** 함수 안에서 굴리면 "전설로 승급하는 주사위 값" 같은
 * 상황을 테스트 코드가 만들어낼 방법이 없다.
 *
 * 실제 굴리기와 골드 차감은 UTDItemUseComponent::ServerRerollOptions 가 한다.
 *
 * ── 굴리는 규칙 ──
 * 등급 R 인 아이템의 세 줄은 이렇게 정해진다.
 *
 *   1줄   R 풀에서 100%
 *   2줄   SameTierLineChance 로 R 풀, 아니면 R-1 풀
 *   3줄   위와 같음
 *
 * 가장 낮은 등급은 아래가 없으므로 세 줄 다 자기 풀에서 나온다.
 */
namespace TDItemOption
{
	/**
	 * 등급 목록을 Tier 순으로 정렬해 돌려준다. 승급과 하위 풀 조회의 기준이다.
	 *
	 * @param RarityTable  DT_OptionRarity. 없으면 빈 배열.
	 */
	TArray<FTDOptionRarityRow> GetSortedRarities(const UDataTable* RarityTable);

	/** 그 등급의 행. 없으면 nullptr. */
	const FTDOptionRarityRow* FindRarity(const TArray<FTDOptionRarityRow>& Sorted, FGameplayTag Rarity);

	/**
	 * 한 단계 위 등급. 최고 등급이면 자기 자신을 그대로 돌려준다.
	 *
	 * 승급 판정에 쓴다 — 최고 등급에서 굴려도 결과가 같아 호출부에 분기가 필요 없다.
	 */
	FGameplayTag GetNextRarity(const TArray<FTDOptionRarityRow>& Sorted, FGameplayTag Rarity);

	/** 한 단계 아래 등급. 가장 낮으면 자기 자신. 2·3번째 줄이 떨어질 곳이다. */
	FGameplayTag GetPreviousRarity(const TArray<FTDOptionRarityRow>& Sorted, FGameplayTag Rarity);

	/**
	 * 재굴림 실제 비용. 착용 레벨제한이 높을수록 비싸다.
	 *
	 * 레벨이 높은 장비일수록 뽑히는 옵션 수치도 크므로, 비용이 같으면 고레벨 장비를
	 * 굴리는 것이 일방적으로 이득이 된다.
	 */
	int32 GetRerollCost(int32 BaseCost, int32 ItemRequiredLevel);

	/**
	 * (풀, 등급) 에서 옵션 하나를 가중치로 뽑는다.
	 *
	 * @param Roll  0~1. 가중치 합에 곱해 뽑을 지점을 정한다.
	 * @return      뽑힌 OptionId. 그 조합에 행이 하나도 없으면 NAME_None.
	 */
	FName PickOption(const UDataTable* PoolTable, FName PoolId, FGameplayTag Rarity, float Roll);

	/**
	 * 뽑힌 옵션의 수치를 정한다. MinValue~MaxValue 사이를 선형으로 고른다.
	 *
	 * @param Roll  0~1.
	 */
	float RollOptionValue(const FTDOptionDefinitionRow& Definition, float Roll);

	/**
	 * 세 줄을 통째로 굴린다.
	 *
	 * @param Rarity      굴릴 등급. 승급 판정은 이 함수 밖에서 끝내고 결과를 넣는다.
	 * @param LineRolls   줄마다 세 개씩 필요하다 — [등급판정, 옵션선택, 수치].
	 *                    개수가 모자라면 그만큼만 굴린다.
	 * @return            굴려진 옵션들. 풀이 비어 있으면 그 줄은 빠지므로
	 *                    LineCount 보다 적을 수 있다.
	 */
	TArray<FTDItemOption> RollLines(const UDataTable* PoolTable, const UDataTable* DefinitionTable,
		const TArray<FTDOptionRarityRow>& Sorted, FName PoolId, FGameplayTag Rarity,
		int32 LineCount, TArrayView<const float> LineRolls);
}
