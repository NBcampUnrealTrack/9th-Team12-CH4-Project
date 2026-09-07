#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TDEnhanceRow.h"
#include "Items/TDItemTypes.h"
#include "Save/TDPlayerSaveData.h"
#include "TDInventoryComponent.generated.h"

class UDataTable;
struct FTDItemRow;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnInventoryChanged);

/** 골드가 바뀌었을 때. 아이템 변경과 나눠 둔다 — 골드만 보는 UI 가 매번 다시 그리지 않도록. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOnGoldChanged, int32, NewGold);

/** 강화 요청이 처리됐을 때. 서버·클라이언트 양쪽에서 불린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTDOnItemEnhanced,
	int32, SlotIndex, ETDEnhanceResult, Result, int32, NewEnhanceLevel);

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

	/**
	 * 위 상한을 블루프린트에서도 읽게 연다. constexpr 은 C++ 에서만 보인다.
	 *
	 * UI 가 "40 / 200" 처럼 남은 확장 여지를 보여주거나, 상한에 닿았을 때
	 * 확장 버튼을 잠그는 데 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Inventory")
	static int32 GetMaxSlotCapacity() { return MaxSlotCapacity; }

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

	/**
	 * 인벤토리에 있는 아이템을 한 단계 강화한다. **장착 중인 아이템은 대상이 아니다** —
	 * 먼저 해제해야 한다. 두 컨테이너(인벤토리·장착) 양쪽에 강화 로직을 두면
	 * 같은 판정을 두 번 유지해야 해서, 강화는 인벤토리 하나로 좁혔다.
	 *
	 * 실패해도 골드는 차감된다 — 시도 자체의 대가다. 결과는 ClientItemEnhanced 로 돌아온다.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Inventory")
	void ServerEnhanceItem(int32 SlotIndex);

	/**
	 * 강화 요청의 결과. **양쪽 다 알려준다** — 성공해도 화면에 반응이 없으면
	 * "버튼이 안 먹었나" 하게 된다(ETDZoneTravelResult 와 같은 이유).
	 */
	UPROPERTY(BlueprintAssignable, Category = "TD|Inventory")
	FTDOnItemEnhanced OnItemEnhanced;

	UFUNCTION(Client, Reliable)
	void ClientItemEnhanced(int32 SlotIndex, ETDEnhanceResult Result, int32 NewEnhanceLevel);

	/**
	 * 강화 한 단계의 확률·비용을 읽는다. **UI 가 "다음 강화" 안내를 띄울 때 쓴다.**
	 *
	 * Level 은 올리려는 목표 단수다 — 3강짜리를 4강으로 만들 때의 확률을 보려면 4 를 넣는다.
	 * 테이블에 없는 단수면 false 다. 최고 강화 단수를 넘었다는 뜻이므로 버튼을 잠그면 된다.
	 *
	 * 클라이언트에서 불러도 정확하다 — DT_Enhance 는 콘텐츠라 양쪽이 같은 것을 갖고 있다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Inventory")
	bool GetEnhanceInfo(int32 Level, FTDEnhanceRow& OutRow) const;

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

	// ── 골드 ──────────────────────────────────────────────
	// 아이템이 아니라 숫자 하나지만, 소지품이라는 점에서 인벤토리와 성격이 같아
	// 별도 컴포넌트를 만들지 않고 여기 둔다. 상점·거래소도 둘을 함께 다룬다.

	UFUNCTION(BlueprintPure, Category = "TD|Inventory")
	int32 GetGold() const { return Gold; }

	UFUNCTION(BlueprintPure, Category = "TD|Inventory")
	bool CanAfford(int32 Cost) const { return Cost >= 0 && Gold >= Cost; }

	/**
	 * 골드를 더한다. **서버 전용이며 음수를 받지 않는다.**
	 *
	 * 차감을 SpendGold 로 나눈 이유는 실수를 막기 위해서다 — AddGold(-100) 하나로
	 * 처리하면 계산 결과가 음수인 줄 모르고 넘겨 잔액이 마이너스가 될 수 있다.
	 *
	 * @return 실제로 늘었으면 true.
	 */
	bool AddGold(int32 Amount);

	/** 골드를 쓴다. **모자라면 아무것도 하지 않고 false.** 부분 차감은 없다. */
	bool SpendGold(int32 Amount);

	/** 골드가 바뀌었을 때. 인벤토리 변경과 나눠 두어 UI 가 필요한 쪽만 구독한다. */
	UPROPERTY(BlueprintAssignable, Category = "TD|Inventory")
	FTDOnGoldChanged OnGoldChanged;

	// ── 세이브 구조체 ─────────────────────────────────────

	void WriteSaveData(FTDPlayerSaveData& Out) const;
	void ReadSaveData(const FTDPlayerSaveData& In);

protected:
	virtual void BeginPlay() override;

	/** DT_ItemDefinition. 스택 여부와 버리기 가능 여부를 여기서 읽는다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Inventory")
	TObjectPtr<UDataTable> ItemTable;

	/** DT_Enhance. Level 열로 찾는다 — RowName 은 "Lv07" 같은 편집용 별칭일 뿐이다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Inventory")
	TObjectPtr<UDataTable> EnhanceTable;

private:
	/**
	 * 획득 알림을 이 플레이어의 채팅창에 보낸다. **서버에서만 의미가 있다.**
	 *
	 * 세이브를 읽을 때는 불리지 않는다 — ReadSaveData 는 AddItem 을 거치지 않고
	 * 컨테이너를 통째로 갈아끼우기 때문이다. 접속할 때마다 가진 아이템이
	 * 전부 "획득" 으로 올라오면 창이 못 쓰게 된다.
	 */
	void NotifyLoot(const FString& Message) const;

	UFUNCTION()
	void OnRep_Gold();

	/**
	 * 소지 골드. 남의 지갑을 볼 이유가 없으므로 인벤토리와 같은 조건이다(D34).
	 *
	 * 상한을 두는 이유는 오버플로 때문이다. int32 최대치 근처에서 더하면
	 * 음수로 뒤집혀 "골드가 마이너스" 가 되고, 그 시점에는 원인을 찾기 어렵다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_Gold)
	int32 Gold = 0;

	/** 소지 골드 상한. 20억이면 int32 오버플로에서 충분히 멀다. */
	static constexpr int32 MaxGold = 2000000000;

	const FTDItemRow* FindItemRow(FName ItemId) const;

	/**
	 * DT_Enhance 에서 Level 열로 찾는다. RowName 이 아니라 이 값으로 찾는 이유는
	 * ZoneEnvironment 와 같다 — 시트에서 RowName 을 자유롭게 쓰기 위해서다.
	 */
	const struct FTDEnhanceRow* FindEnhanceRow(int32 Level) const;

	/** 비어 있는 가장 앞 칸. 없으면 INDEX_NONE. */
	int32 FindEmptySlotIndex() const;

	FTDItemInstance* FindMutableBySlot(int32 SlotIndex);

	bool HasAuthorityToModify() const;

	/** 서버에서 내용을 바꾼 뒤 호출한다. 복제 표시와 알림을 함께 처리한다. */
	void MarkItemDirty(FTDItemInstance& Item);
	void MarkContainerDirty();

	UPROPERTY(Replicated)
	FTDItemContainer ItemContainer;

	/**
	 * 확장권으로 늘어나므로 플레이어의 선택이고, 따라서 저장 대상이다.
	 *
	 * OnRep 을 다는 이유는 서버와 클라이언트의 알림 경로가 다르기 때문이다.
	 * 서버는 SetSlotCapacity 안에서 직접 브로드캐스트하지만, 클라이언트는
	 * 값이 도착하는 것 말고는 알 방법이 없다 — 없으면 확장 직후 UI 가
	 * 옛 칸 수를 그대로 그리다가 재접속해야 반영된다(골드와 같은 구조).
	 */
	UPROPERTY(ReplicatedUsing = OnRep_SlotCapacity)
	int32 SlotCapacity = 40;

	UFUNCTION()
	void OnRep_SlotCapacity();
};
