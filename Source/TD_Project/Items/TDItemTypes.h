#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "TDItemTypes.generated.h"

class UActorComponent;

/**
 * 강화 요청 한 번의 결과. UI 에 그대로 돌려준다.
 *
 * `ETDEnhanceOutcome`(TDEnhanceStatics.h)과 다른 열거형인 이유는 성격이 다르기 때문이다.
 * 그쪽은 "주사위를 굴린 결과"(성공/하락/변화없음)이고, 이쪽은 "요청 자체가 성립했는가"
 * 까지 포함한다 — 골드가 모자라거나 이미 최고 레벨이면 주사위를 굴리지도 않는다.
 * ETDZoneTravelResult 와 같은 이유로 문구가 아니라 enum 이다 — UI 가 문구를 정한다.
 */
UENUM(BlueprintType)
enum class ETDEnhanceResult : uint8
{
	Success			UMETA(DisplayName = "성공"),
	Downgraded		UMETA(DisplayName = "하락"),
	FailedNoChange	UMETA(DisplayName = "실패"),

	/** 이 레벨을 넘는 시도는 테이블에 없다. */
	MaxLevelReached	UMETA(DisplayName = "최대 강화"),

	NotEnoughGold	UMETA(DisplayName = "골드 부족"),
	ItemNotFound	UMETA(DisplayName = "아이템 없음"),

	/** DT_Enhance 가 지정되지 않았거나 해당 레벨 행이 없다. 데이터 누락이라 플레이어 잘못이 아니다. */
	InternalError	UMETA(DisplayName = "내부 오류")
};

/**
 * 장신구에 굴려진 추가 옵션 하나.
 *
 * 어떤 범위에서 굴렸는지는 DT_OptionDefinition 에 있고, 여기에는 결과만 담긴다.
 * 이 값은 개체마다 다르므로 반드시 저장해야 한다 — 다시 굴리면 다른 값이 나온다.
 */
USTRUCT(BlueprintType)
struct FTDItemOption
{
	GENERATED_BODY()

	/** DT_OptionDefinition 의 RowName. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Item")
	FName OptionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Item")
	float Value = 0.f;

	bool operator==(const FTDItemOption& Other) const
	{
		return OptionId == Other.OptionId && FMath::IsNearlyEqual(Value, Other.Value);
	}
};

/**
 * 인벤토리에 실제로 들어 있는 아이템 한 칸.
 *
 * FFastArraySerializerItem 을 상속해 변경분만 복제된다. 그 대가로 배열 순서가
 * 서버와 클라이언트에서 다를 수 있으므로, 화면 위치는 배열 인덱스가 아니라
 * SlotIndex 로 결정해야 한다.
 */
USTRUCT(BlueprintType)
struct FTDItemInstance : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** DT_ItemDefinition 의 RowName. 이름이 길어도 FName 자체는 8바이트다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Item", meta=(GetOptions="TD_Project.TDAccountDummyData.GetItemOptions"))
	FName ItemId;

	/** 인벤토리에서의 자리. 배열 순서와 무관하며 UI 는 이 값으로 그린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Item")
	int32 SlotIndex = INDEX_NONE;

	/** 스택 수량. 겹칠 수 없는 아이템은 항상 1이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Item")
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Item")
	int32 EnhanceLevel = 0;

	/**
	 * 추가 옵션의 품질. 아이템 자체의 등급(FTDItemRow::Rarity)과는 다른 축이다.
	 *
	 * 아이템의 격은 고정이지만 이 값은 옵션을 굴릴 때마다 확률로 오른다.
	 * 올라간 뒤에는 DT_OptionPool 에서 그 등급의 옵션만 뽑히므로, 같은 반지라도
	 * 개체마다 붙는 옵션의 격이 달라진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Item")
	FGameplayTag OptionRarity;

	/**
	 * 추가 옵션. 장신구만 가지며 굴리는 로직은 아직 없다.
	 *
	 * 필드를 미리 두는 이유는 저장 형식 때문이다. 나중에 추가하면 SaveVersion 을 올리고
	 * 옛 데이터를 변환하는 분기를 만들어야 하지만, 지금 넣으면 비용이 없다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Item")
	TArray<FTDItemOption> Options;

	bool IsValid() const { return !ItemId.IsNone() && Count > 0; }

	/** 같은 칸에 겹칠 수 있는 물건인지. 강화나 옵션이 붙었으면 개체가 다르다. */
	bool CanStackWith(const FTDItemInstance& Other) const
	{
		return ItemId == Other.ItemId
			&& EnhanceLevel == 0 && Other.EnhanceLevel == 0
			&& Options.IsEmpty() && Other.Options.IsEmpty();
	}

	// ── FastArray 콜백 ────────────────────────────────────
	// 클라이언트에서 항목이 추가·변경·제거될 때 불린다. UI 갱신 알림을 여기서 낸다.

	void PreReplicatedRemove(const struct FTDItemContainer& InArray);
	void PostReplicatedAdd(const struct FTDItemContainer& InArray);
	void PostReplicatedChange(const struct FTDItemContainer& InArray);
};

/**
 * 아이템 목록의 복제 단위.
 *
 * 일반 TArray 를 복제하면 한 칸만 바뀌어도 배열 전체가 다시 전송된다.
 * FastArray 는 변경된 항목만 보내고, 빈 슬롯은 아예 담지 않으므로
 * 인벤토리를 40칸에서 200칸으로 늘려도 전송량이 늘지 않는다.
 */
USTRUCT(BlueprintType)
struct FTDItemContainer : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FTDItemInstance> Items;

	/**
	 * 콜백에서 알림을 낼 대상. 복제하지 않으며 컴포넌트가 직접 채운다.
	 *
	 * 인벤토리와 장착 목록이 같은 컨테이너 타입을 쓰므로 구체 타입으로 못 박지 않는다.
	 */
	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent = nullptr;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FTDItemInstance, FTDItemContainer>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FTDItemContainer> : public TStructOpsTypeTraitsBase2<FTDItemContainer>
{
	enum { WithNetDeltaSerializer = true };
};
