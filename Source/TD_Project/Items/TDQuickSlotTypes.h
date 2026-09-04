#pragma once

#include "CoreMinimal.h"
#include "TDQuickSlotTypes.generated.h"

/** 퀵슬롯에 무엇이 들어 있는가. */
UENUM(BlueprintType)
enum class ETDQuickSlotType : uint8
{
	Empty	UMETA(DisplayName = "비어 있음"),
	Item	UMETA(DisplayName = "아이템"),
	Skill	UMETA(DisplayName = "스킬")
};

/**
 * 퀵슬롯 한 칸.
 *
 * ── 아이템을 ItemId 로 가리키는 이유 ──
 * 인벤토리 슬롯 번호를 가리키면 아이템을 다른 칸으로 옮기거나 정렬하는 순간
 * 퀵슬롯이 통째로 어긋난다. ID 로 두면 인벤토리 어디에 있든 찾아 쓰고,
 * 다 써서 없어져도 회색으로 남아 있다가 다시 채우면 살아난다.
 *
 * 개수는 담지 않는다 — 인벤토리를 보면 알 수 있는 파생값이라
 * 여기 저장하면 두 곳이 어긋날 수 있다(D12 와 같은 이유).
 */
USTRUCT(BlueprintType)
struct FTDQuickSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TD|QuickSlot")
	ETDQuickSlotType Type = ETDQuickSlotType::Empty;

	/** Type 에 따라 DT_ItemDefinition 의 RowName 이거나 스킬 식별자다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|QuickSlot")
	FName Id;

	bool IsEmpty() const { return Type == ETDQuickSlotType::Empty || Id.IsNone(); }

	bool operator==(const FTDQuickSlot& Other) const
	{
		return Type == Other.Type && Id == Other.Id;
	}
};
