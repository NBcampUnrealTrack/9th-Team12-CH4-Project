#include "Items/TDQuickSlotComponent.h"

#include "Core/TDGameplayTags.h"
#include "Data/TDItemRow.h"
#include "Items/TDInventoryComponent.h"
#include "Items/TDItemTypes.h"
#include "Items/TDItemUseComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/TDPlayerState.h"

UTDQuickSlotComponent::UTDQuickSlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// 길이를 처음부터 고정한다. 빈 칸을 담지 않으면 인덱스가 화면 위치와 어긋난다.
	Slots.SetNum(SlotCount);
}

void UTDQuickSlotComponent::BeginPlay()
{
	Super::BeginPlay();

	// 세이브가 짧은 배열을 넣었거나 SlotCount 를 늘렸을 때를 대비한다.
	if (Slots.Num() != SlotCount)
	{
		Slots.SetNum(SlotCount);
	}
}

void UTDQuickSlotComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 남의 퀵슬롯을 볼 이유가 없다. 인벤토리와 같은 조건이다(D34).
	DOREPLIFETIME_CONDITION(UTDQuickSlotComponent, Slots, COND_OwnerOnly);
}

ATDPlayerState* UTDQuickSlotComponent::GetOwnerPlayerState() const
{
	return Cast<ATDPlayerState>(GetOwner());
}

// ── 조회 ──────────────────────────────────────────────────

FTDQuickSlot UTDQuickSlotComponent::GetSlot(int32 Index) const
{
	// 범위를 벗어나면 빈 슬롯. UI 가 매번 인덱스를 검사하지 않아도 된다.
	return Slots.IsValidIndex(Index) ? Slots[Index] : FTDQuickSlot();
}

int32 UTDQuickSlotComponent::GetSlotItemCount(int32 Index) const
{
	const FTDQuickSlot Slot = GetSlot(Index);
	if (Slot.Type != ETDQuickSlotType::Item || Slot.Id.IsNone())
	{
		return 0;
	}

	const ATDPlayerState* PlayerState = GetOwnerPlayerState();
	const UTDInventoryComponent* Inventory =
		PlayerState ? PlayerState->GetInventoryComponent() : nullptr;

	return Inventory != nullptr ? Inventory->GetItemCount(Slot.Id) : 0;
}

// ── 요청 ──────────────────────────────────────────────────

void UTDQuickSlotComponent::ServerSetSlot_Implementation(int32 Index, ETDQuickSlotType Type, FName Id)
{
	if (!Slots.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning, TEXT("퀵슬롯: %d 번은 없는 슬롯이다. (0~%d)"), Index, SlotCount - 1);
		return;
	}

	// 빈 것을 등록하려는 것은 지우기와 같다.
	if (Type == ETDQuickSlotType::Empty || Id.IsNone())
	{
		ServerClearSlot(Index);
		return;
	}

	if (Type == ETDQuickSlotType::Item)
	{
		const ATDPlayerState* PlayerState = GetOwnerPlayerState();
		const UTDInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
		const FTDItemRow* Definition = Inventory ? Inventory->FindItemDefinition(Id) : nullptr;
		if (!Definition || Definition->ItemType != TDTags::Item_Type_Consumable.GetTag()
			|| Inventory->GetItemCount(Id) <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("퀵슬롯: 실제 보유한 소비 아이템만 등록할 수 있습니다. 아이템 '%s'를 확인하세요."), *Id.ToString());
			return;
		}
	}
	else
	{
		// 아이템도 빈 것도 아닌 값이 오면 조용히 버린다. 지금은 그런 타입이 없지만,
		// 타입이 늘었을 때 검증 없이 통과하는 일을 막는다.
		return;
	}

	// 검증에 실패한 요청은 기존 등록과 다른 슬롯을 변경하지 않는다.
	// 하나가 두 칸을 차지하면 어느 쪽을 눌러도 같아 혼란스럽다.
	ClearDuplicates(Index, Type, Id);

	Slots[Index].Type = Type;
	Slots[Index].Id = Id;

	NotifyChanged();
}

void UTDQuickSlotComponent::ServerClearSlot_Implementation(int32 Index)
{
	if (!Slots.IsValidIndex(Index) || Slots[Index].IsEmpty())
	{
		return;
	}

	Slots[Index] = FTDQuickSlot();

	NotifyChanged();
}

void UTDQuickSlotComponent::ServerSwapSlots_Implementation(int32 FromIndex, int32 ToIndex)
{
	if (!Slots.IsValidIndex(FromIndex) || !Slots.IsValidIndex(ToIndex) || FromIndex == ToIndex)
	{
		return;
	}

	Slots.Swap(FromIndex, ToIndex);

	NotifyChanged();
}

void UTDQuickSlotComponent::ServerUseSlot_Implementation(int32 Index)
{
	const FTDQuickSlot Slot = GetSlot(Index);
	if (Slot.IsEmpty())
	{
		return;
	}

	ATDPlayerState* PlayerState = GetOwnerPlayerState();
	if (PlayerState == nullptr)
	{
		return;
	}

	switch (Slot.Type)
	{
	case ETDQuickSlotType::Item:
	{
		const UTDInventoryComponent* Inventory = PlayerState->GetInventoryComponent();
		UTDItemUseComponent* ItemUse = PlayerState->GetItemUseComponent();

		if (Inventory == nullptr || ItemUse == nullptr)
		{
			return;
		}

		// ItemId 로 등록돼 있으므로 인벤토리에서 그 아이템이 있는 칸을 찾는다.
		// 여러 칸에 나뉘어 있으면 앞쪽부터 쓴다.
		int32 FoundSlot = INDEX_NONE;

		for (const FTDItemInstance& Item : Inventory->GetItems())
		{
			if (Item.ItemId == Slot.Id)
			{
				FoundSlot = Item.SlotIndex;
				break;
			}
		}

		if (FoundSlot == INDEX_NONE)
		{
			// 다 쓴 물약 자리를 누른 경우다. 오류를 띄울 일이 아니다.
			return;
		}

		ItemUse->UseItem(FoundSlot);
		break;
	}

	default:
		break;
	}
}

// ── 내부 ──────────────────────────────────────────────────

void UTDQuickSlotComponent::ClearDuplicates(int32 KeepIndex, ETDQuickSlotType Type, FName Id)
{
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (i != KeepIndex && Slots[i].Type == Type && Slots[i].Id == Id)
		{
			Slots[i] = FTDQuickSlot();
		}
	}
}

void UTDQuickSlotComponent::NotifyChanged()
{
	// 서버에서는 OnRep 이 불리지 않으므로 직접 알린다.
	OnQuickSlotsChanged.Broadcast();

	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UTDQuickSlotComponent::OnRep_Slots()
{
	OnQuickSlotsChanged.Broadcast();
}

// ── 세이브 ────────────────────────────────────────────────

void UTDQuickSlotComponent::WriteSaveData(TArray<FTDQuickSlot>& Out) const
{
	Out = Slots;
}

void UTDQuickSlotComponent::ReadSaveData(const TArray<FTDQuickSlot>& In)
{
	Slots = In;

	// 저장 당시보다 슬롯이 늘었을 수 있다. 모자란 만큼 빈 칸으로 채운다.
	if (Slots.Num() != SlotCount)
	{
		Slots.SetNum(SlotCount);
	}

	NotifyChanged();
}
