#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "TDItemRow.generated.h"

class UTexture2D;

/**
 * DT_ItemDefinition 의 행. 아이템 종류 하나의 정적 정의다.
 *
 * 여기 있는 값은 모든 플레이어에게 동일하다. 강화 레벨이나 추가 옵션처럼
 * 개체마다 달라지는 것은 FTDItemInstance 쪽에 있다.
 *
 * 식별자는 RowName 이다. ItemId 필드를 따로 두면 같은 값이 두 곳에 존재하게 된다.
 */
USTRUCT(BlueprintType)
struct FTDItemRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/**
	 * 툴팁에 보이는 설명. 비어 있으면 UI 가 설명 줄을 그리지 않는다.
	 *
	 * 스탯 수치는 여기 적지 않는다 — 그쪽은 DT_ItemStat 에서 읽어 UI 가 만든다.
	 * 여기 적으면 밸런스를 고칠 때마다 문장도 함께 고쳐야 하고, 둘이 어긋난다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText Description;

	/** Item.Type.{Accessory|Consumable|Misc|Quest} */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FGameplayTag ItemType;

	/** 하드 참조로 두면 테이블을 여는 것만으로 모든 아이콘이 메모리에 올라온다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;

	// ── 스택 ──────────────────────────────────────────────

	/**
	 * 같은 아이템끼리 한 칸에 겹쳐지는가.
	 *
	 * 추가 옵션이나 강화가 붙는 아이템은 개체마다 달라서 겹칠 수 없다.
	 * 즉 장신구는 항상 false 여야 하며, 이 규칙이 깨진 데이터는 검증 스크립트가 잡아야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Stack")
	bool bStackable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Stack", meta = (EditCondition = "bStackable", ClampMin = "1"))
	int32 MaxStackSize = 1;

	/** 퀘스트 아이템은 false. 서버가 버리기 요청을 거부하는 근거가 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bCanDiscard = true;

	/**
	 * 사용·장착에 필요한 레벨. 0 이면 제한이 없다.
	 *
	 * 인벤토리에 넣는 것은 막지 않는다. 레벨이 모자라도 획득은 되고, 쓰거나 끼울 때만 걸린다.
	 * 획득 자체를 막으면 파티원이 대신 먹어주는 흐름이나 창고 보관이 불가능해진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0"))
	int32 RequiredLevel = 0;

	// ── 장신구 전용 ───────────────────────────────────────

	/**
	 * 아이템 자체의 등급. 이름 색상이나 판매가처럼 아이템의 격을 나타내며 **바뀌지 않는다**.
	 *
	 * 추가 옵션이 어느 등급에서 뽑히는지는 이 값이 아니라 인스턴스의 OptionRarity 가 정한다.
	 * 전설 반지라도 옵션 품질은 일반부터 시작해 굴리면서 오를 수 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Accessory")
	FGameplayTag Rarity;

	/**
	 * 획득했을 때의 추가 옵션 품질. 비워두면 가장 낮은 등급부터 시작한다.
	 *
	 * "처음부터 희귀 옵션이 붙어 나오는 반지" 같은 특별한 아이템에만 채우면 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Accessory")
	FGameplayTag InitialOptionRarity;

	/**
	 * DT_OptionPool 참조. 레벨대별로 풀을 나누고 아이템마다 직접 지정한다.
	 *
	 * 아이템 레벨로 구간을 자동 선택하는 방식도 가능하지만, 그렇게 하면
	 * "왜 이 옵션이 나왔지"를 데이터만 봐서는 추적할 수 없다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Accessory")
	FName OptionPoolId;

	/**
	 * 소속 세트. 비어 있으면 세트에 속하지 않는다.
	 *
	 * 같은 SetId 를 몇 개 착용했는지에 따라 DT_ItemSetBonus 의 단계 효과가 붙는다.
	 * 세트 이름 표시는 DT_ItemSet 에서 읽는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Accessory")
	FName SetId;
};
