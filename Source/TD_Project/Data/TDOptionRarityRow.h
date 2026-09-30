#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "TDOptionRarityRow.generated.h"

/**
 * DT_OptionRarity 의 행. 추가 옵션 등급 하나의 규칙이다.
 *
 * ── 왜 테이블이 따로 필요한가 ──
 * Item.Rarity.* 태그는 서로 형제라 **순서를 표현하지 못한다.** "Rare 아래가 Common" 은
 * 태그만 봐서는 알 수 없는데, 2·3번째 줄을 한 단계 아래 풀에서 뽑으려면 그 순서가 있어야 한다.
 *
 * 순서를 코드에 박지 않은 이유는 나머지 값들 때문이다. 확률과 비용은 운영하며 계속
 * 만지게 되는데, 그때마다 빌드를 다시 하게 만들 이유가 없다.
 *
 *   RowName    Rarity                 Tier  SameTierLineChance  UpgradeChance  RerollCost
 *   Common     Item.Rarity.Common     0     0.0                 0.10           1000
 *   Rare       Item.Rarity.Rare       1     0.5                 0.05           3000
 *   Epic       Item.Rarity.Epic       2     0.5                 0.02          10000
 *   Legendary  Item.Rarity.Legendary  3     0.5                 0.0           30000
 */
USTRUCT(BlueprintType)
struct FTDOptionRarityRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Item.Rarity.* 중 하나. 이 행이 어느 등급의 규칙인지 가리킨다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option Rarity")
	FGameplayTag Rarity;

	/**
	 * 등급의 높낮이. 0 이 가장 낮다. **비어 있거나 겹치면 승급과 하위 풀 조회가 깨진다.**
	 *
	 * 값 자체에는 의미가 없고 대소 관계만 쓴다 — 0·1·2·3 이든 10·20·30·40 이든 같다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option Rarity", meta = (ClampMin = "0"))
	int32 Tier = 0;

	/**
	 * 2·3번째 줄이 **현재 등급** 풀에서 뽑힐 확률. 나머지는 한 단계 아래 풀에서 뽑는다.
	 *
	 * 첫 줄은 언제나 현재 등급이라 이 값의 영향을 받지 않는다.
	 * 가장 낮은 등급(Tier 0)은 아래가 없으므로 세 줄 다 자기 풀에서 나온다 —
	 * 그래서 이 값이 무시된다.
	 *
	 * **넉넉하게 잡아야 한다.** 줄 하나만 남기고 다시 굴리는 기능이 없어서,
	 * 세 줄을 통째로 새로 뽑는다. 0.5 면 세 줄 모두 상위가 25%, 최소 한 줄은 75% 다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option Rarity",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SameTierLineChance = 0.5f;

	/**
	 * 재굴림 한 번에 **이 등급에서 한 단계 위로** 오를 확률.
	 *
	 * 일반 반지도 계속 굴리면 언젠가 전설이 된다(D31). 이것이 재굴림의 목표이며,
	 * 없으면 수치만 반복해서 뽑는 일이 된다.
	 *
	 * 최고 등급 행은 0 으로 둔다. 위가 없으므로 값이 있어도 아무 일도 일어나지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option Rarity",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UpgradeChance = 0.f;

	/**
	 * 재굴림 기본 비용. **실제 비용은 아이템의 착용 레벨제한만큼 배가 된다.**
	 *
	 * 레벨이 높은 장비일수록 옵션 수치도 커지므로, 비용만 같으면 고레벨 장비를
	 * 굴리는 것이 일방적으로 이득이 된다. 배율은 UTDItemOptionSettings 가 정한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option Rarity", meta = (ClampMin = "0"))
	int32 RerollCost = 0;
};
