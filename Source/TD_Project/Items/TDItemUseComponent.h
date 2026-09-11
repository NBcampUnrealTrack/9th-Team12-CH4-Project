#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TDOptionRarityRow.h"
#include "GameplayTagContainer.h"
#include "Items/TDItemTypes.h"
#include "Save/TDPlayerSaveData.h"
#include "Stats/TDStatTypes.h"
#include "TDItemUseComponent.generated.h"

class UDataTable;
class UTDInventoryComponent;
class UTDProgressionComponent;
class UTDStatComponent;
struct FTDItemRow;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnEquipmentChanged);

/** 추가 옵션 재굴림의 결과. */
UENUM(BlueprintType)
enum class ETDRerollResult : uint8
{
	Success				UMETA(DisplayName = "성공"),

	/** 성공했고 등급까지 올랐다. 연출을 다르게 낼 자리다. */
	SuccessUpgraded		UMETA(DisplayName = "성공(등급 상승)"),

	ItemNotFound		UMETA(DisplayName = "아이템 없음"),

	/** 장신구가 아니다. 추가 옵션은 장신구만 가진다. */
	NotAccessory		UMETA(DisplayName = "장신구 아님"),

	NotEnoughGold		UMETA(DisplayName = "골드 부족"),

	/** DT_ItemDefinition 의 OptionPoolId 가 비었거나, 테이블·설정이 지정되지 않았다. */
	InternalError		UMETA(DisplayName = "내부 오류")
};

/**
 * 재굴림 결과 알림. **UI 가 구독할 지점이다.**
 *
 * 굴려진 옵션은 아이템에 들어가 복제되므로 따로 싣지 않는다 — 인벤토리·장착 갱신
 * 알림을 받고 그 아이템을 다시 읽으면 된다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDOnOptionsRerolled,
	ETDRerollResult, Result, FGameplayTag, NewRarity);

/**
 * 아이템을 장착하고 사용하는 컴포넌트.
 *
 * 인벤토리와 같은 액터(APlayerState)에 붙는다. 장착 상태는 저장 대상이고 사망해도
 * 유지돼야 하므로 Pawn 에 두면 리스폰마다 전부 벗겨진다.
 *
 * 장착으로 생기는 스탯 변화는 전부 **하나의 모디파이어 소스**로 묶어 등록한다.
 * 반지 하나를 빼도 세트 개수가 달라져 다른 아이템의 효과까지 바뀌므로,
 * 개별로 관리하는 것보다 통째로 다시 만드는 편이 단순하다.
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDItemUseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDItemUseComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 장신구 칸 수. 고정이다. */
	static constexpr int32 EquipSlotCount = 6;

	// ── 장착 (서버 전용) ──────────────────────────────────

	/**
	 * 인벤토리의 아이템을 장착 칸에 끼운다.
	 *
	 * 칸이 이미 차 있으면 원래 있던 것이 인벤토리로 돌아간다. 인벤토리에 자리가 없으면
	 * 실패하며, 이때 두 아이템 모두 원래 위치에 그대로 남는다.
	 */
	bool EquipItem(int32 InventorySlot, int32 EquipSlot);

	/** 장착을 풀어 인벤토리로 되돌린다. 인벤토리에 자리가 없으면 실패한다. */
	bool UnequipItem(int32 EquipSlot);

	/** 소비 아이템을 쓴다. 효과는 DT_ItemUseEffect 에 정의된 만큼 적용되고 개수가 하나 준다. */
	bool UseItem(int32 InventorySlot);

	// ── UI 진입점 ─────────────────────────────────────────
	// UI 는 항상 이쪽을 부른다. 서버에서 호출하면 네트워크를 타지 않고 즉시 실행되므로
	// "지금 서버인가 클라인가"를 판단할 필요가 없다.
	//
	// 검증은 위의 함수들이 이미 하고 있다. 클라이언트가 임의의 슬롯 번호를 보내도
	// 권한·범위·아이템 종류·레벨·중복 확인에서 걸린다.

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Equipment")
	void ServerEquipItem(int32 InventorySlot, int32 EquipSlot);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Equipment")
	void ServerUnequipItem(int32 EquipSlot);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Equipment")
	void ServerUseItem(int32 InventorySlot);

	// ── 조회 ──────────────────────────────────────────────

	const FTDItemInstance* GetEquipped(int32 EquipSlot) const;

	const TArray<FTDItemInstance>& GetEquippedItems() const { return EquippedContainer.Items; }

	/** 세트 조각을 몇 개 착용 중인지. UI 의 "2/4" 표시에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "TD|Equipment")
	int32 GetSetPieceCount(FName SetId) const;

	/**
	 * 강화가 이 아이템의 스탯을 몇 배로 만드는지. **1강당 상승폭은 아이템마다 다르다** —
	 * DT_ItemDefinition 의 RequiredLevel 이 높을수록 커진다(2%/강 ~ 7%/강).
	 *
	 * 장착 여부와 무관하게 답한다. 인벤토리에 있는 아이템의 툴팁에도 쓸 수 있고,
	 * EnhanceLevel + 1 을 넣으면 "다음 강화에 성공하면 얼마가 되는지" 미리보기가 된다.
	 *
	 * 강화 단수가 0 이면 1.0 이다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Equipment")
	float GetEnhanceMultiplier(FName ItemId, int32 EnhanceLevel) const;

	UPROPERTY(BlueprintAssignable, Category = "TD|Equipment")
	FTDOnEquipmentChanged OnEquipmentChanged;

	void BroadcastEquipmentChanged();

	// ── 추가 옵션 (잠재능력) ──────────────────────────────
	//
	// 굴리기가 여기 있는 이유는 장착 중인 아이템도 굴릴 수 있어야 하기 때문이다.
	// 장착 아이템은 인벤토리가 아니라 이 컴포넌트의 EquippedContainer 에 들어 있고,
	// 굴린 뒤 스탯을 다시 등록하는 RefreshEquipmentModifiers 도 여기 있다.
	//
	// 강화(UTDInventoryComponent::ServerEnhanceItem)와 짝이지만 자리가 다른 것은
	// 강화가 인벤토리 아이템만 대상으로 하기 때문이다.

	/**
	 * 추가 옵션을 다시 굴린다. 골드를 치르고 세 줄을 통째로 새로 뽑는다.
	 *
	 * **줄 하나만 남기는 기능은 없다.** 그래서 2·3번째 줄이 현재 등급에서 나올 확률
	 * (DT_OptionRarity.SameTierLineChance)을 넉넉하게 잡아야 한다.
	 *
	 * 굴릴 때마다 낮은 확률로 **등급이 오른다**(D31). 일반 반지도 계속 굴리면 언젠가
	 * 전설이 되며, 이것이 재굴림의 목표다.
	 *
	 * @param SlotIndex   bEquipped 면 장착 칸(0~5), 아니면 인벤토리 칸.
	 * @param bEquipped   착용 중인 것을 굴리는가. 장착 아이템은 인벤토리에 없으므로
	 *                    칸 번호만으로는 어느 쪽인지 알 수 없다.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Equipment")
	void ServerRerollOptions(int32 SlotIndex, bool bEquipped);

	/**
	 * 재굴림에 드는 골드. 버튼 옆에 값을 띄우는 데 쓴다.
	 *
	 * 등급 기본값(DT_OptionRarity.RerollCost)에 아이템의 착용 레벨제한 배율이 붙는다.
	 * 굴릴 수 없는 아이템이면 0.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Equipment")
	int32 GetRerollCost(int32 SlotIndex, bool bEquipped) const;

	UPROPERTY(BlueprintAssignable, Category = "TD|Equipment")
	FTDOnOptionsRerolled OnOptionsRerolled;

	UFUNCTION(Client, Reliable)
	void ClientOptionsRerolled(ETDRerollResult Result, FGameplayTag NewRarity);

	/**
	 * DT_OptionDefinition. 툴팁이 굴려진 옵션을 글자로 만들 때 쓴다.
	 *
	 * 같은 테이블 참조를 UI 가 따로 들지 않게 하려고 열어 둔다 — 두 곳에 두면
	 * 한쪽만 지정해 놓고 왜 옵션이 안 보이는지 찾게 된다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Equipment")
	UDataTable* GetOptionDefinitionTable() const { return OptionDefinitionTable; }

	/** DT_ItemStat. 위와 같은 이유로 열어 둔다 — 툴팁이 강화된 스탯을 보여줄 때 쓴다. */
	UFUNCTION(BlueprintPure, Category = "TD|Equipment")
	UDataTable* GetItemStatTable() const { return ItemStatTable; }

	// ── 세이브 구조체 ─────────────────────────────────────

	void WriteSaveData(FTDPlayerSaveData& Out) const;
	void ReadSaveData(const FTDPlayerSaveData& In);

protected:
	virtual void BeginPlay() override;

	/** DT_ItemStat. 장비가 고정으로 주는 스탯. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Equipment")
	TObjectPtr<UDataTable> ItemStatTable;

	/** DT_ItemSetBonus. 세트 단계 효과. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Equipment")
	TObjectPtr<UDataTable> SetBonusTable;

	/** DT_OptionDefinition. 굴려진 추가 옵션이 어떤 스탯인지 여기서 읽는다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Equipment")
	TObjectPtr<UDataTable> OptionDefinitionTable;

	/** DT_ItemUseEffect. 소비 아이템의 효과. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Equipment")
	TObjectPtr<UDataTable> UseEffectTable;

private:
	/**
	 * 굴리기에 필요한 것을 한 번에 모은다. 굴리기와 비용 조회가 같은 판단을 두 번
	 * 하지 않도록 나눠 둔 것이다.
	 *
	 * @param OutItem     굴릴 대상. 비용 조회에서는 읽기만 한다.
	 * @return            굴릴 수 없는 이유. Success 면 나머지 출력이 전부 유효하다.
	 */
	ETDRerollResult PrepareReroll(int32 SlotIndex, bool bEquipped,
		const FTDItemInstance*& OutItem, const FTDItemRow*& OutDefinition,
		TArray<FTDOptionRarityRow>& OutSortedRarities, int32& OutCost) const;

	/**
	 * 장착 중인 모든 것에서 모디파이어를 다시 만들어 스탯 컴포넌트에 등록한다.
	 *
	 * 고정 옵션 + 굴려진 추가 옵션 + 세트 단계 효과를 한 배열로 합친다.
	 * 세트 판정이 전체 장착 상태에 달려 있어 부분 갱신이 성립하지 않는다.
	 */
	void RefreshEquipmentModifiers();

	/**
	 * 소비 효과 하나를 실제로 적용한다. 태그를 보고 무엇을 할지 정한다.
	 *
	 * @return 효과가 실제로 일어났으면 true. 풀피에서 회복 포션을 쓰는 경우처럼
	 *         아무 일도 없었으면 false 이고, 그때는 아이템을 소모하지 않는다.
	 */
	bool ApplyUseEffect(FGameplayTag EffectTag, float Value);

	const FTDItemRow* FindItemRow(FName ItemId) const;

	UTDInventoryComponent* GetInventory() const;
	UTDStatComponent* GetStatComponent() const;
	UTDProgressionComponent* GetProgression() const;

	/**
	 * 이 컴포넌트의 주인이 조종하는 캐릭터.
	 *
	 * 회복은 PlayerState 가 아니라 월드의 캐릭터에게 적용해야 한다 —
	 * ASC 는 PlayerState 에 있지만 죽고 사는 것은 아바타다.
	 * 캐릭터를 고르기 전에는 Pawn 이 없으므로 nullptr 이 정상이다.
	 */
	AActor* GetOwnerCharacter() const;

	bool HasAuthorityToModify() const;

	FTDItemInstance* FindMutableEquipped(int32 EquipSlot);

	UPROPERTY(Replicated)
	FTDItemContainer EquippedContainer;

	/** RefreshEquipmentModifiers 가 등록한 소스. 갱신할 때 이 핸들로 이전 것을 걷어낸다. */
	FTDStatSourceHandle EquipmentSourceHandle;
};
