#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/TDItemTypes.h"
#include "Save/TDPlayerSaveData.h"
#include "TDInventoryComponent.generated.h"

class UDataTable;
struct FTDItemRow;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnInventoryChanged);

/**
 * 플레이어의 소지품을 관리하는 컴포넌트.
 *
 * 스탯 컴포넌트와 같은 이유로 APlayerState 에 붙인다. Pawn 에 두면 사망할 때
 * 인벤토리가 통째로 사라진다.
 *
 * 조작은 전부 서버에서 일어난다. 클라이언트가 보낸 슬롯 번호를 그대로 믿으면
 * 범위 밖이나 남의 칸을 지정할 수 있으므로, 모든 진입점에서 권한과 범위를 확인한다.
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 확장의 상한. 없으면 클라이언트가 터무니없는 확장을 요청했을 때 막을 근거가 없다. */
	static constexpr int32 MaxSlotCapacity = 200;

	// ── 조작 (서버 전용) ──────────────────────────────────

	/**
	 * 아이템을 넣는다. 겹칠 수 있으면 기존 칸을 먼저 채우고 남은 만큼 새 칸을 쓴다.
	 *
	 * 전부 들어갈 자리가 없으면 **하나도 넣지 않고** 실패한다. 일부만 넣으면
	 * 남은 수량을 어디로 되돌릴지 호출한 쪽이 다시 결정해야 하기 때문이다.
	 */
	bool AddItem(FName ItemId, int32 Count = 1);

	/** 슬롯에서 수량만큼 덜어낸다. 퀘스트 아이템처럼 버릴 수 없는 것은 거부된다. */
	bool RemoveItem(int32 SlotIndex, int32 Count = 1);

	/** 슬롯끼리 옮긴다. 대상이 비었으면 이동, 겹칠 수 있으면 합치고, 아니면 교환한다. */
	bool MoveItem(int32 FromSlot, int32 ToSlot);

	/** 칸 수를 늘린다. 줄이는 것은 허용하지 않는다 — 넘치는 아이템을 처리할 방법이 없다. */
	void SetSlotCapacity(int32 NewCapacity);

	// ── UI 진입점 ─────────────────────────────────────────
	// 서버에서 호출하면 네트워크를 타지 않고 즉시 실행되므로, UI 는 자신이 서버인지
	// 클라인지 판단하지 않고 항상 이쪽을 부르면 된다.
	//
	// AddItem 과 SetSlotCapacity 는 여기 없다. 클라이언트가 스스로 아이템을 지급하거나
	// 칸을 늘릴 수 있게 되기 때문이다. 그 둘은 드롭 획득이나 확장권 사용처럼
	// 서버 로직에서만 호출돼야 한다.

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Inventory")
	void ServerMoveItem(int32 FromSlot, int32 ToSlot);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Inventory")
	void ServerDropItem(int32 SlotIndex, int32 Count = 1);

	// ── 다른 시스템이 쓰는 진입점 (서버 전용) ────────────
	// 장착처럼 인벤토리 밖으로 아이템이 오가는 경우를 위한 것이다.
	// 버리기와 달리 bCanDiscard 를 보지 않는다 — 없어지는 게 아니라 자리를 옮기는 것이므로.

	/** 슬롯의 아이템을 통째로 꺼내 인벤토리에서 지운다. 꺼낸 내용은 OutItem 에 담긴다. */
	bool TakeItemAt(int32 SlotIndex, FTDItemInstance& OutItem);

	/** 비어 있는 특정 슬롯에 넣는다. 이미 차 있으면 실패한다. */
	bool PutItemAt(int32 SlotIndex, const FTDItemInstance& Item);

	/** 가장 앞의 빈 슬롯에 넣는다. 자리가 없으면 실패한다. */
	bool PutItemInFirstEmptySlot(const FTDItemInstance& Item);

	/** 사용으로 개수를 줄인다. 버리기가 아니므로 퀘스트 아이템도 소모될 수 있다. */
	bool ConsumeItemAt(int32 SlotIndex, int32 Count = 1);

	/** 아이템 정의 조회. 다른 컴포넌트가 같은 테이블을 또 들고 있지 않도록 열어둔다. */
	const FTDItemRow* FindItemDefinition(FName ItemId) const;

	// ── 조회 ──────────────────────────────────────────────

	const FTDItemInstance* FindBySlot(int32 SlotIndex) const;

	/** 같은 아이템이 여러 칸에 나뉘어 있어도 합쳐서 센다. */
	UFUNCTION(BlueprintPure, Category = "TD|Inventory")
	int32 GetItemCount(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "TD|Inventory")
	int32 GetSlotCapacity() const { return SlotCapacity; }

	UFUNCTION(BlueprintPure, Category = "TD|Inventory")
	int32 GetUsedSlotCount() const { return ItemContainer.Items.Num(); }

	const TArray<FTDItemInstance>& GetItems() const { return ItemContainer.Items; }

	/** 내용이 바뀌었을 때. 서버·클라이언트 양쪽에서 불린다. */
	UPROPERTY(BlueprintAssignable, Category = "TD|Inventory")
	FTDOnInventoryChanged OnInventoryChanged;

	/** FastArray 콜백이 알림을 낼 때 쓴다. */
	void BroadcastInventoryChanged();

	// ── 세이브 구조체 ─────────────────────────────────────

	void WriteSaveData(FTDPlayerSaveData& Out) const;
	void ReadSaveData(const FTDPlayerSaveData& In);

protected:
	virtual void BeginPlay() override;

	/** DT_ItemDefinition. 스택 여부와 버리기 가능 여부를 여기서 읽는다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Inventory")
	TObjectPtr<UDataTable> ItemTable;

private:
	const FTDItemRow* FindItemRow(FName ItemId) const;

	/** 비어 있는 가장 앞 칸. 없으면 INDEX_NONE. */
	int32 FindEmptySlotIndex() const;

	FTDItemInstance* FindMutableBySlot(int32 SlotIndex);

	bool HasAuthorityToModify() const;

	/** 서버에서 내용을 바꾼 뒤 호출한다. 복제 표시와 알림을 함께 처리한다. */
	void MarkItemDirty(FTDItemInstance& Item);
	void MarkContainerDirty();

	UPROPERTY(Replicated)
	FTDItemContainer ItemContainer;

	/** 확장권으로 늘어나므로 플레이어의 선택이고, 따라서 저장 대상이다. */
	UPROPERTY(Replicated)
	int32 SlotCapacity = 40;
};
