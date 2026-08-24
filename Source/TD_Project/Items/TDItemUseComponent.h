#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

	UPROPERTY(BlueprintAssignable, Category = "TD|Equipment")
	FTDOnEquipmentChanged OnEquipmentChanged;

	void BroadcastEquipmentChanged();

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
	 * 장착 중인 모든 것에서 모디파이어를 다시 만들어 스탯 컴포넌트에 등록한다.
	 *
	 * 고정 옵션 + 굴려진 추가 옵션 + 세트 단계 효과를 한 배열로 합친다.
	 * 세트 판정이 전체 장착 상태에 달려 있어 부분 갱신이 성립하지 않는다.
	 */
	void RefreshEquipmentModifiers();

	/** 소비 효과 하나를 실제로 적용한다. 태그를 보고 무엇을 할지 정한다. */
	void ApplyUseEffect(FGameplayTag EffectTag, float Value);

	const FTDItemRow* FindItemRow(FName ItemId) const;

	UTDInventoryComponent* GetInventory() const;
	UTDStatComponent* GetStatComponent() const;
	UTDProgressionComponent* GetProgression() const;

	bool HasAuthorityToModify() const;

	FTDItemInstance* FindMutableEquipped(int32 EquipSlot);

	UPROPERTY(Replicated)
	FTDItemContainer EquippedContainer;

	/** RefreshEquipmentModifiers 가 등록한 소스. 갱신할 때 이 핸들로 이전 것을 걷어낸다. */
	FTDStatSourceHandle EquipmentSourceHandle;
};
