#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/TDQuickSlotTypes.h"
#include "TDQuickSlotComponent.generated.h"

class ATDPlayerState;

/** 퀵슬롯이 바뀌었을 때. UI 가 이걸 받아 다시 그린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnQuickSlotsChanged);

/**
 * 퀵슬롯. `ATDPlayerState` 에 붙는다.
 *
 * 배치는 **캐릭터별 저장 대상**이다. 전사의 1번과 법사의 1번이 같을 이유가 없으므로
 * 계정이 아니라 FTDPlayerSaveData 에 들어간다.
 *
 * 반면 **어느 키를 누를지는 여기가 아니다.** 그쪽은 Enhanced Input 의
 * UEnhancedInputUserSettings 가 로컬에 저장한다 — 캐릭터를 바꿔도 손가락 위치는
 * 그대로여야 하기 때문이다.
 *
 *   퀵슬롯 1번에 무엇이 들어 있나   →  여기 (캐릭터별)
 *   퀵슬롯 1번을 어느 키로 쓰나      →  Enhanced Input (PC 별)
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDQuickSlotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDQuickSlotComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 슬롯 개수. 늘리려면 IA_QuickSlot* 에셋도 함께 늘려야 한다. */
	static constexpr int32 SlotCount = 6;

	// ── 조회 ──────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "TD|QuickSlot")
	const TArray<FTDQuickSlot>& GetSlots() const { return Slots; }

	/** 범위를 벗어나면 빈 슬롯을 돌려준다. UI 가 매번 인덱스를 검사하지 않아도 된다. */
	UFUNCTION(BlueprintPure, Category = "TD|QuickSlot")
	FTDQuickSlot GetSlot(int32 Index) const;

	/**
	 * 그 슬롯의 아이템이 인벤토리에 몇 개 있는가. 아이템이 아니면 0.
	 *
	 * 개수를 슬롯에 저장하지 않고 매번 세는 이유는 인벤토리가 진실의 원천이기
	 * 때문이다. 저장하면 아이템을 버렸을 때 갱신을 빠뜨려 어긋난다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|QuickSlot")
	int32 GetSlotItemCount(int32 Index) const;

	// ── 요청 ──────────────────────────────────────────────

	/**
	 * 슬롯에 등록한다. 같은 것이 다른 슬롯에 이미 있으면 그쪽은 비워진다 —
	 * 하나의 아이템이 두 칸을 차지하면 어느 쪽을 눌러도 같아 혼란스럽다.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|QuickSlot")
	void ServerSetSlot(int32 Index, ETDQuickSlotType Type, FName Id);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|QuickSlot")
	void ServerClearSlot(int32 Index);

	/** 두 슬롯을 맞바꾼다. UI 에서 드래그로 자리를 옮길 때 쓴다. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|QuickSlot")
	void ServerSwapSlots(int32 FromIndex, int32 ToIndex);

	/**
	 * 슬롯을 사용한다. 키 입력이 이걸 부른다.
	 *
	 * 아이템이면 인벤토리에서 같은 ItemId 를 찾아 쓴다. 없으면 조용히 실패한다 —
	 * 다 쓴 물약 자리를 눌렀을 때 오류를 띄울 필요는 없다.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|QuickSlot")
	void ServerUseSlot(int32 Index);

	UPROPERTY(BlueprintAssignable, Category = "TD|QuickSlot")
	FTDOnQuickSlotsChanged OnQuickSlotsChanged;

	// ── 세이브 ────────────────────────────────────────────

	void WriteSaveData(TArray<FTDQuickSlot>& Out) const;
	void ReadSaveData(const TArray<FTDQuickSlot>& In);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Slots();

	ATDPlayerState* GetOwnerPlayerState() const;

	/** 같은 것이 등록된 다른 슬롯을 비운다. 서버 전용. */
	void ClearDuplicates(int32 KeepIndex, ETDQuickSlotType Type, FName Id);

	void NotifyChanged();

	/**
	 * 남의 퀵슬롯을 볼 이유가 없다. 인벤토리와 같은 조건이다(D34).
	 *
	 * 배열 길이는 항상 SlotCount 로 고정한다. 빈 칸을 담지 않으면 인덱스가
	 * 화면 위치와 어긋나기 때문이다 — FastArray 를 쓰는 인벤토리와 다른 점이다(D26).
	 */
	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FTDQuickSlot> Slots;
};
