#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Stats/TDStatTypes.h"
#include "TDOptionRow.generated.h"

/**
 * DT_OptionDefinition 의 행. 장신구에 붙을 수 있는 추가 옵션 하나의 정의다.
 *
 * 실제로 굴려진 수치는 여기가 아니라 FTDItemInstance::Options 에 들어간다.
 * 이 테이블은 "어떤 범위에서 굴릴 것인가"만 정한다.
 */
USTRUCT(BlueprintType)
struct FTDOptionDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** UI 표시용. "화염 피해 +N" 처럼 수치가 들어갈 자리를 포함할 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option")
	FGameplayTag StatTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option")
	ETDModOp Op = ETDModOp::Added;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option")
	float MinValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option")
	float MaxValue = 0.f;
};

/**
 * DT_OptionPool 의 행. 어떤 풀의 어떤 등급에서 이 옵션이 뽑힐 수 있는지를 나타낸다.
 *
 * 풀은 레벨대별로 나누고 아이템이 직접 지정한다(FTDItemRow::OptionPoolId).
 * 같은 풀 안에서도 등급이 다르면 뽑히는 옵션이 달라지므로, 필터는 (PoolId, Rarity) 두 개다.
 *
 *   RowName             PoolId      Rarity              OptionId          Weight
 *   Lv10_Common_Atk     Pool_Lv10   Item.Rarity.Common  Opt_Damage_Small  100
 *   Lv10_Rare_Atk       Pool_Lv10   Item.Rarity.Rare    Opt_Damage_Large   30
 */
USTRUCT(BlueprintType)
struct FTDOptionPoolRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option Pool")
	FName PoolId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option Pool")
	FGameplayTag Rarity;

	/** DT_OptionDefinition 의 RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option Pool")
	FName OptionId;

	/** 뽑힐 가중치. 클수록 자주 나온다. 0 이하면 뽑히지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option Pool", meta = (ClampMin = "0"))
	float Weight = 1.f;
};
