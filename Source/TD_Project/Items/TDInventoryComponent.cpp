#include "Items/TDInventoryComponent.h"

#include "Data/TDItemRow.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

namespace
{
	const TCHAR* ItemTableContext = TEXT("UTDInventoryComponent");
}

// FastArray 콜백은 인벤토리와 장착 목록이 함께 쓰므로 TDItemTypes.cpp 에 모아두었다.

UTDInventoryComponent::UTDInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTDInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// 콜백이 알림을 낼 대상을 알려준다. 복제되지 않는 필드라 양쪽에서 각자 채워야 한다.
	ItemContainer.OwnerComponent = this;

	// 지정 누락은 조용히 실패하므로 시작할 때 한 번 짚어준다.
	// 매 조회마다 로그를 내면 스팸이 되고, 시작 시점이면 원인을 바로 알 수 있다.
	if (ItemTable == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: InventoryComponent 의 ItemTable(DT_ItemDefinition) 이 지정되지 않았다. "
				 "아이템 획득이 전부 실패한다."),
			*GetNameSafe(GetOwner()));
	}
}

void UTDInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 남의 인벤토리를 알 필요는 없다. 소유 커넥션에만 보낸다.
	DOREPLIFETIME_CONDITION(UTDInventoryComponent, ItemContainer, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UTDInventoryComponent, SlotCapacity, COND_OwnerOnly);
}

bool UTDInventoryComponent::HasAuthorityToModify() const
{
	const AActor* Owner = GetOwner();
	return Owner != nullptr && Owner->HasAuthority();
}

const FTDItemRow* UTDInventoryComponent::FindItemRow(FName ItemId) const
{
	if (ItemTable == nullptr || ItemId.IsNone())
	{
		return nullptr;
	}

	return ItemTable->FindRow<FTDItemRow>(ItemId, ItemTableContext, /*bWarnIfRowMissing=*/false);
}

const FTDItemInstance* UTDInventoryComponent::FindBySlot(int32 SlotIndex) const
{
	return ItemContainer.Items.FindByPredicate(
		[SlotIndex](const FTDItemInstance& Item) { return Item.SlotIndex == SlotIndex; });
}

FTDItemInstance* UTDInventoryComponent::FindMutableBySlot(int32 SlotIndex)
{
	return ItemContainer.Items.FindByPredicate(
		[SlotIndex](const FTDItemInstance& Item) { return Item.SlotIndex == SlotIndex; });
}

int32 UTDInventoryComponent::FindEmptySlotIndex() const
{
	for (int32 Slot = 0; Slot < SlotCapacity; ++Slot)
	{
		if (FindBySlot(Slot) == nullptr)
		{
			return Slot;
		}
	}

	return INDEX_NONE;
}

int32 UTDInventoryComponent::GetItemCount(FName ItemId) const
{
	int32 Total = 0;
	for (const FTDItemInstance& Item : ItemContainer.Items)
	{
		if (Item.ItemId == ItemId)
		{
			Total += Item.Count;
		}
	}

	return Total;
}

void UTDInventoryComponent::BroadcastInventoryChanged()
{
	OnInventoryChanged.Broadcast();
}

void UTDInventoryComponent::MarkItemDirty(FTDItemInstance& Item)
{
	ItemContainer.MarkItemDirty(Item);
	BroadcastInventoryChanged();
}

void UTDInventoryComponent::MarkContainerDirty()
{
	ItemContainer.MarkArrayDirty();
	BroadcastInventoryChanged();
}

bool UTDInventoryComponent::AddItem(FName ItemId, int32 Count)
{
	if (!HasAuthorityToModify())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AddItem: 서버 권한이 없다. 클라이언트에서는 아이템을 지급할 수 없다."));
		return false;
	}

	if (Count <= 0)
	{
		return false;
	}

	const FTDItemRow* Row = FindItemRow(ItemId);
	if (Row == nullptr)
	{
		// 테이블 자체가 없는 것과 행이 없는 것은 원인이 다르므로 구분해서 알린다.
		if (ItemTable == nullptr)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("AddItem: ItemTable 이 지정되지 않았다. BP_PlayerState 의 InventoryComponent 를 확인할 것."));
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("AddItem: DT_ItemDefinition 에 '%s' 행이 없다. RowName 을 확인할 것."), *ItemId.ToString());
		}
		return false;
	}

	const int32 MaxStack = Row->bStackable ? FMath::Max(1, Row->MaxStackSize) : 1;

	// ── 1단계: 전부 들어갈 수 있는지 먼저 따진다 ──
	// 일부만 넣고 실패하면 남은 수량을 어디로 되돌릴지 호출한 쪽이 다시 판단해야 한다.
	int32 Remaining = Count;

	if (Row->bStackable)
	{
		for (const FTDItemInstance& Item : ItemContainer.Items)
		{
			if (Item.ItemId != ItemId || Item.Count >= MaxStack)
			{
				continue;
			}

			Remaining -= (MaxStack - Item.Count);
			if (Remaining <= 0)
			{
				break;
			}
		}
	}

	if (Remaining > 0)
	{
		const int32 NeededSlots = FMath::DivideAndRoundUp(Remaining, MaxStack);
		const int32 FreeSlots = SlotCapacity - ItemContainer.Items.Num();
		if (NeededSlots > FreeSlots)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("AddItem: 칸이 부족하다. '%s' %d개에 %d칸이 필요한데 %d칸만 비어 있다."),
				*ItemId.ToString(), Count, NeededSlots, FreeSlots);
			return false;
		}
	}

	// ── 2단계: 실제로 넣는다 ──
	int32 ToAdd = Count;

	if (Row->bStackable)
	{
		for (FTDItemInstance& Item : ItemContainer.Items)
		{
			if (ToAdd <= 0)
			{
				break;
			}

			if (Item.ItemId != ItemId || Item.Count >= MaxStack)
			{
				continue;
			}

			const int32 Added = FMath::Min(ToAdd, MaxStack - Item.Count);
			Item.Count += Added;
			ToAdd -= Added;

			ItemContainer.MarkItemDirty(Item);
		}
	}

	while (ToAdd > 0)
	{
		const int32 EmptySlot = FindEmptySlotIndex();
		if (EmptySlot == INDEX_NONE)
		{
			// 1단계에서 걸렀어야 하는 상황이다. 여기 도달하면 계산이 틀린 것.
			UE_LOG(LogTemp, Error, TEXT("AddItem: 빈 칸 계산이 어긋났다. ItemId=%s"), *ItemId.ToString());
			break;
		}

		FTDItemInstance NewItem;
		NewItem.ItemId = ItemId;
		NewItem.SlotIndex = EmptySlot;
		NewItem.Count = FMath::Min(ToAdd, MaxStack);
		ToAdd -= NewItem.Count;

		FTDItemInstance& Added = ItemContainer.Items.Add_GetRef(NewItem);
		ItemContainer.MarkItemDirty(Added);
	}

	BroadcastInventoryChanged();
	return true;
}

bool UTDInventoryComponent::RemoveItem(int32 SlotIndex, int32 Count)
{
	if (!HasAuthorityToModify() || Count <= 0)
	{
		return false;
	}

	FTDItemInstance* Item = FindMutableBySlot(SlotIndex);
	if (Item == nullptr)
	{
		return false;
	}

	const FTDItemRow* Row = FindItemRow(Item->ItemId);
	if (Row != nullptr && !Row->bCanDiscard)
	{
		// 퀘스트 아이템 등. 클라이언트가 버리기를 요청해도 여기서 막힌다.
		return false;
	}

	if (Item->Count > Count)
	{
		Item->Count -= Count;
		MarkItemDirty(*Item);
		return true;
	}

	// 남김없이 덜어내면 칸 자체를 비운다.
	ItemContainer.Items.RemoveAll(
		[SlotIndex](const FTDItemInstance& Entry) { return Entry.SlotIndex == SlotIndex; });

	MarkContainerDirty();
	return true;
}

bool UTDInventoryComponent::MoveItem(int32 FromSlot, int32 ToSlot)
{
	if (!HasAuthorityToModify() || FromSlot == ToSlot)
	{
		return false;
	}

	// 클라이언트가 보낸 번호를 그대로 믿지 않는다.
	const bool bValidRange = FromSlot >= 0 && FromSlot < SlotCapacity
		&& ToSlot >= 0 && ToSlot < SlotCapacity;
	if (!bValidRange)
	{
		return false;
	}

	FTDItemInstance* From = FindMutableBySlot(FromSlot);
	if (From == nullptr)
	{
		return false;
	}

	FTDItemInstance* To = FindMutableBySlot(ToSlot);

	// 빈 칸으로 이동
	if (To == nullptr)
	{
		From->SlotIndex = ToSlot;
		MarkItemDirty(*From);
		return true;
	}

	// 겹칠 수 있으면 합친다. 넘치는 만큼은 원래 칸에 남는다.
	if (From->CanStackWith(*To))
	{
		const FTDItemRow* Row = FindItemRow(To->ItemId);
		const int32 MaxStack = (Row && Row->bStackable) ? FMath::Max(1, Row->MaxStackSize) : 1;
		const int32 Movable = FMath::Min(From->Count, MaxStack - To->Count);

		if (Movable > 0)
		{
			To->Count += Movable;
			From->Count -= Movable;

			if (From->Count <= 0)
			{
				ItemContainer.Items.RemoveAll(
					[FromSlot](const FTDItemInstance& Entry) { return Entry.SlotIndex == FromSlot; });
				MarkContainerDirty();
			}
			else
			{
				MarkItemDirty(*From);
			}

			// From 을 지웠을 수 있으므로 To 는 다시 찾는다.
			if (FTDItemInstance* Target = FindMutableBySlot(ToSlot))
			{
				MarkItemDirty(*Target);
			}
			return true;
		}
	}

	// 그 외에는 자리를 맞바꾼다.
	From->SlotIndex = ToSlot;
	To->SlotIndex = FromSlot;
	MarkItemDirty(*From);
	MarkItemDirty(*To);

	return true;
}

void UTDInventoryComponent::SetSlotCapacity(int32 NewCapacity)
{
	if (!HasAuthorityToModify())
	{
		return;
	}

	// 줄이는 것은 막는다. 칸이 줄면 넘치는 아이템을 어디로 보낼지 정해야 하는데,
	// 확장만 허용하면 그 문제가 아예 생기지 않는다.
	if (NewCapacity <= SlotCapacity)
	{
		return;
	}

	SlotCapacity = FMath::Min(NewCapacity, MaxSlotCapacity);
	BroadcastInventoryChanged();
}

// ── UI 진입점 ─────────────────────────────────────────────
// 전송 코드는 UHT 가 생성한다. 여기서는 실제 처리에 넘기기만 한다.

void UTDInventoryComponent::ServerMoveItem_Implementation(int32 FromSlot, int32 ToSlot)
{
	MoveItem(FromSlot, ToSlot);
}

void UTDInventoryComponent::ServerDropItem_Implementation(int32 SlotIndex, int32 Count)
{
	RemoveItem(SlotIndex, Count);
}

const FTDItemRow* UTDInventoryComponent::FindItemDefinition(FName ItemId) const
{
	return FindItemRow(ItemId);
}

bool UTDInventoryComponent::TakeItemAt(int32 SlotIndex, FTDItemInstance& OutItem)
{
	if (!HasAuthorityToModify())
	{
		return false;
	}

	const FTDItemInstance* Found = FindBySlot(SlotIndex);
	if (Found == nullptr)
	{
		return false;
	}

	OutItem = *Found;

	ItemContainer.Items.RemoveAll(
		[SlotIndex](const FTDItemInstance& Entry) { return Entry.SlotIndex == SlotIndex; });

	MarkContainerDirty();
	return true;
}

bool UTDInventoryComponent::PutItemAt(int32 SlotIndex, const FTDItemInstance& Item)
{
	if (!HasAuthorityToModify() || !Item.IsValid())
	{
		return false;
	}

	if (SlotIndex < 0 || SlotIndex >= SlotCapacity || FindBySlot(SlotIndex) != nullptr)
	{
		return false;
	}

	FTDItemInstance Placed = Item;
	Placed.SlotIndex = SlotIndex;

	FTDItemInstance& Added = ItemContainer.Items.Add_GetRef(Placed);
	MarkItemDirty(Added);

	return true;
}

bool UTDInventoryComponent::PutItemInFirstEmptySlot(const FTDItemInstance& Item)
{
	const int32 EmptySlot = FindEmptySlotIndex();
	if (EmptySlot == INDEX_NONE)
	{
		return false;
	}

	return PutItemAt(EmptySlot, Item);
}

bool UTDInventoryComponent::ConsumeItemAt(int32 SlotIndex, int32 Count)
{
	if (!HasAuthorityToModify() || Count <= 0)
	{
		return false;
	}

	FTDItemInstance* Item = FindMutableBySlot(SlotIndex);
	if (Item == nullptr || Item->Count < Count)
	{
		return false;
	}

	// 버리기(RemoveItem)와 달리 bCanDiscard 를 보지 않는다.
	// 퀘스트 아이템을 버릴 수는 없어도 써서 없앨 수는 있어야 한다.
	if (Item->Count > Count)
	{
		Item->Count -= Count;
		MarkItemDirty(*Item);
		return true;
	}

	ItemContainer.Items.RemoveAll(
		[SlotIndex](const FTDItemInstance& Entry) { return Entry.SlotIndex == SlotIndex; });

	MarkContainerDirty();
	return true;
}

void UTDInventoryComponent::WriteSaveData(FTDPlayerSaveData& Out) const
{
	Out.InventorySlotCapacity = SlotCapacity;
	Out.InventoryItems = ItemContainer.Items;
}

void UTDInventoryComponent::ReadSaveData(const FTDPlayerSaveData& In)
{
	SlotCapacity = FMath::Clamp(In.InventorySlotCapacity, 1, MaxSlotCapacity);

	ItemContainer.Items = In.InventoryItems;
	MarkContainerDirty();
}
