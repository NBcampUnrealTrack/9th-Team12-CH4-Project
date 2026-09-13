#include "Items/TDInventoryComponent.h"

#include "Data/TDEnhanceRow.h"
#include "Data/TDItemRow.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Game/TDGameMode.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerState.h"
#include "Items/TDEnhanceStatics.h"
#include "Net/UnrealNetwork.h"
#include "Core/TDGameplayTags.h"

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

	// 남의 지갑도 볼 이유가 없다. 같은 조건으로 보낸다.
	DOREPLIFETIME_CONDITION(UTDInventoryComponent, Gold, COND_OwnerOnly);
}

// ── 골드 ──────────────────────────────────────────────────

bool UTDInventoryComponent::AddGold(int32 Amount)
{
	if (!HasAuthorityToModify())
	{
		UE_LOG(LogTemp, Warning, TEXT("AddGold 는 서버에서만 호출해야 한다."));
		return false;
	}

	if (Amount <= 0)
	{
		// 차감은 SpendGold 를 쓴다. 여기로 음수가 오면 호출부의 실수다.
		UE_LOG(LogTemp, Warning,
			TEXT("AddGold: 0 이하(%d)는 받지 않는다. 차감은 SpendGold 를 쓸 것."), Amount);
		return false;
	}

	// 상한에 걸리면 남는 만큼만 들어간다. 넘치는 분은 버린다 —
	// 여기서 실패시키면 보상을 아예 못 받게 되어 더 나쁘다.
	const int32 NewGold = (Gold > MaxGold - Amount) ? MaxGold : Gold + Amount;
	if (NewGold == Gold)
	{
		return false;
	}

	// 상한에 걸려 일부만 들어갔을 수 있으므로 실제로 늘어난 만큼을 알린다.
	const int32 ActualGain = NewGold - Gold;
	Gold = NewGold;

	// 서버에서는 OnRep 이 불리지 않으므로 직접 알린다.
	OnGoldChanged.Broadcast(Gold);

	NotifyLoot(FString::Printf(TEXT("%d 골드 획득"), ActualGain));

	return true;
}

bool UTDInventoryComponent::SpendGold(int32 Amount)
{
	if (!HasAuthorityToModify())
	{
		UE_LOG(LogTemp, Warning, TEXT("SpendGold 는 서버에서만 호출해야 한다."));
		return false;
	}

	if (Amount <= 0 || !CanAfford(Amount))
	{
		// 부분 차감은 하지 않는다. 살 수 없으면 아무것도 하지 않는다(D28 과 같은 원칙).
		UE_LOG(LogTemp, Log,
			TEXT("SpendGold 실패: %d 골드가 필요한데 %d 뿐이다."), Amount, Gold);
		return false;
	}

	Gold -= Amount;
	OnGoldChanged.Broadcast(Gold);

	return true;
}

void UTDInventoryComponent::NotifyLoot(const FString& Message) const
{
	const APlayerState* OwnerState = Cast<APlayerState>(GetOwner());
	APlayerController* Controller = OwnerState ? OwnerState->GetPlayerController() : nullptr;

	if (Controller == nullptr)
	{
		return;
	}

	// GameMode 를 거치는 이유는 채팅이 나가는 길을 한 곳으로 모으기 위해서다.
	// 나중에 로그나 차단 목록이 붙으면 그쪽만 고치면 된다.
	if (ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr)
	{
		GameMode->SendSystemMessage(Controller, ETDChatChannel::Loot, Message);
	}
}

void UTDInventoryComponent::OnRep_Gold()
{
	OnGoldChanged.Broadcast(Gold);
}

void UTDInventoryComponent::OnRep_SlotCapacity()
{
	// 칸 수가 늘면 화면의 빈 칸도 늘어야 한다. 아이템 변경과 같은 알림을 쓴다 —
	// UI 입장에서는 "인벤토리를 다시 그려라" 로 똑같기 때문이다.
	BroadcastInventoryChanged();
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

const FTDEnhanceRow* UTDInventoryComponent::FindEnhanceRow(int32 Level) const
{
	if (EnhanceTable == nullptr)
	{
		return nullptr;
	}

	// Level 열로 찾는다. RowName 은 "Lv07" 같은 편집용 별칭일 뿐이다.
	const FTDEnhanceRow* Found = nullptr;

	EnhanceTable->ForeachRow<FTDEnhanceRow>(TEXT("FindEnhanceRow"),
		[&Found, Level](const FName&, const FTDEnhanceRow& Row)
		{
			if (Found == nullptr && Row.Level == Level)
			{
				Found = &Row;
			}
		});

	return Found;
}

bool UTDInventoryComponent::GetEnhanceInfo(int32 Level, FTDEnhanceRow& OutRow) const
{
	const FTDEnhanceRow* Row = FindEnhanceRow(Level);
	if (Row == nullptr)
	{
		return false;
	}

	// 포인터를 그대로 넘기지 않는다. 테이블이 다시 임포트되면 행 주소가 바뀌므로,
	// 값을 복사해 주는 편이 호출한 쪽에서 오래 들고 있어도 안전하다.
	OutRow = *Row;
	return true;
}

void UTDInventoryComponent::ServerEnhanceItem_Implementation(int32 SlotIndex)
{
	if (!HasAuthorityToModify())
	{
		return;
	}

	const FTDItemInstance* Item = FindBySlot(SlotIndex);
	const int32 CurrentLevel = Item ? Item->EnhanceLevel : 0;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("직접 강화 요청은 지원하지 않습니다. 대장간 강화창을 이용하세요."));

	ClientItemEnhanced(
		SlotIndex,
		ETDEnhanceResult::InternalError,
		CurrentLevel);
}

void UTDInventoryComponent::ClientItemEnhanced_Implementation(int32 SlotIndex,
	ETDEnhanceResult Result, int32 NewEnhanceLevel)
{
	// 문구는 만들지 않는다. UI 가 이 델리게이트를 받아 자기 형식으로 표시한다.
	OnItemEnhanced.Broadcast(SlotIndex, Result, NewEnhanceLevel);

	// UI 가 붙기 전까지는 로그로만 확인한다.
	UE_LOG(LogTemp, Log, TEXT("강화 결과: 슬롯 %d — %s (%d강)"),
		SlotIndex, *UEnum::GetDisplayValueAsText(Result).ToString(), NewEnhanceLevel);
}

bool UTDInventoryComponent::SetItemOptions(int32 SlotIndex, FGameplayTag OptionRarity,
	const TArray<FTDItemOption>& Options)
{
	if (!HasAuthorityToModify())
	{
		return false;
	}

	FTDItemInstance* Item = FindMutableBySlot(SlotIndex);
	if (Item == nullptr)
	{
		return false;
	}

	Item->OptionRarity = OptionRarity;
	Item->Options = Options;

	MarkItemDirty(*Item);
	return true;
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
	if (HasAuthorityToModify())
	{
		InventoryRevision =
			InventoryRevision >= MAX_int64
				? 1
				: InventoryRevision + 1;
	}

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

bool UTDInventoryComponent::CanAddItems(
	const TMap<FName, int32>& Items) const
{
	if (!HasAuthorityToModify() || Items.IsEmpty())
	{
		return false;
	}

	int64 TotalNeededSlots = 0;

	for (const TPair<FName, int32>& Pair : Items)
	{
		if (Pair.Key.IsNone() || Pair.Value <= 0)
		{
			return false;
		}

		const FTDItemRow* Row = FindItemRow(Pair.Key);

		if (Row == nullptr)
		{
			return false;
		}

		const int64 MaxStack = Row->bStackable
			? FMath::Max(1, Row->MaxStackSize)
			: 1;

		int64 Remaining = Pair.Value;

		if (Row->bStackable)
		{
			for (const FTDItemInstance& Item : ItemContainer.Items)
			{
				if (Item.ItemId == Pair.Key && Item.Count < MaxStack)
				{
					Remaining -= MaxStack - Item.Count;

					if (Remaining <= 0)
					{
						break;
					}
				}
			}
		}

		if (Remaining > 0)
		{
			TotalNeededSlots +=
				(Remaining + MaxStack - 1) / MaxStack;
		}

		if (TotalNeededSlots
			> static_cast<int64>(SlotCapacity - ItemContainer.Items.Num()))
		{
			return false;
		}
	}

	return true;
}

bool UTDInventoryComponent::AddItems(
	const TMap<FName, int32>& Items)
{
	if (!CanAddItems(Items))
	{
		return false;
	}

	// CanAddItems가 모든 종류를 합쳐 검사했으므로 아래 AddItem은 전부 성공합니다.
	// 게임 스레드에서 연속 실행되어 중간에 다른 요청이 끼어들 수 없습니다.
	for (const TPair<FName, int32>& Pair : Items)
	{
		if (!AddItem(Pair.Key, Pair.Value))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("AddItems: 사전 공간 검사 뒤 지급에 실패했다. ItemId=%s Count=%d"),
				*Pair.Key.ToString(),
				Pair.Value);
			return false;
		}
	}

	return true;
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

	// 위에서 이미 찾아 둔 행을 그대로 쓴다. 없으면 함수가 진작 돌아갔다.
	// 표시명이 비어 있으면 RowName 을 쓴다 — 데이터가 덜 채워졌을 때
	// 알림이 사라지는 것보다 "HPotion_Low x3" 이라도 보이는 편이 낫다.
	const FString DisplayName = Row->DisplayName.IsEmpty()
		? ItemId.ToString()
		: Row->DisplayName.ToString();

	NotifyLoot(FString::Printf(TEXT("%s x%d 획득"), *DisplayName, Count));

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
	Out.Gold = Gold;
}

void UTDInventoryComponent::ReadSaveData(const FTDPlayerSaveData& In)
{
	SlotCapacity = FMath::Clamp(In.InventorySlotCapacity, 1, MaxSlotCapacity);

	ItemContainer.Items = In.InventoryItems;
	MarkContainerDirty();

	Gold = FMath::Clamp(In.Gold, 0, MaxGold);
	OnGoldChanged.Broadcast(Gold);
}

ETDEnhanceResult UTDInventoryComponent::EnhanceItemForService(
	int32 SlotIndex,
	int32& OutNewLevel)
{
	OutNewLevel = 0;

	if (!HasAuthorityToModify())
	{
		return ETDEnhanceResult::InternalError;
	}

	FTDItemInstance* Item = FindMutableBySlot(SlotIndex);

	if (Item == nullptr)
	{
		return ETDEnhanceResult::ItemNotFound;
	}

	OutNewLevel = Item->EnhanceLevel;

	const FTDItemRow* Definition =
		FindItemDefinition(Item->ItemId);

	if (Definition == nullptr
		|| Definition->ItemType
			!= TDTags::Item_Type_Accessory.GetTag()
		|| Definition->bStackable
		|| Item->Count != 1)
	{
		return ETDEnhanceResult::InternalError;
	}

	if (EnhanceTable == nullptr
		|| Item->EnhanceLevel < 0
		|| Item->EnhanceLevel >= MAX_int32)
	{
		return ETDEnhanceResult::InternalError;
	}

	const int32 TargetLevel = Item->EnhanceLevel + 1;

	const FTDEnhanceRow* FoundRow =
		FindEnhanceRow(TargetLevel);

	if (FoundRow == nullptr)
	{
		return ETDEnhanceResult::MaxLevelReached;
	}

	// 판정 도중 사용할 값을 복사합니다.
	const FTDEnhanceRow Rule = *FoundRow;

	if (Rule.Cost < 0
		|| !FMath::IsFinite(Rule.SuccessRate)
		|| Rule.SuccessRate < 0.0f
		|| Rule.SuccessRate > 1.0f
		|| !FMath::IsFinite(Rule.DowngradeChanceOnFail)
		|| Rule.DowngradeChanceOnFail < 0.0f
		|| Rule.DowngradeChanceOnFail > 1.0f
		|| Rule.MinDowngradeTiers < 0
		|| Rule.MaxDowngradeTiers < Rule.MinDowngradeTiers)
	{
		return ETDEnhanceResult::InternalError;
	}

	if (!CanAfford(Rule.Cost))
	{
		return ETDEnhanceResult::NotEnoughGold;
	}

	const FTDEnhanceRollResult RollResult =
		TDEnhance::Roll(
			Rule,
			FMath::FRand(),
			FMath::FRand(),
			FMath::FRand());

	ETDEnhanceResult Result =
		ETDEnhanceResult::FailedNoChange;

	int32 NewLevel = Item->EnhanceLevel;

	switch (RollResult.Outcome)
	{
	case ETDEnhanceOutcome::Success:
		NewLevel = TargetLevel;
		Result = ETDEnhanceResult::Success;
		break;

	case ETDEnhanceOutcome::Downgraded:
		NewLevel = FMath::Max(
			0,
			Item->EnhanceLevel - RollResult.DowngradeTiers);
		Result = ETDEnhanceResult::Downgraded;
		break;

	case ETDEnhanceOutcome::FailedNoChange:
	default:
		Result = ETDEnhanceResult::FailedNoChange;
		break;
	}

	/*
	 * 골드와 아이템을 모두 바꾼 뒤 변경 알림을 보냅니다.
	 * 알림을 받은 다른 시스템이 절반만 변경된 상태를 읽지 않게 합니다.
	 */
	Gold -= Rule.Cost;
	Item->EnhanceLevel = NewLevel;
	OutNewLevel = NewLevel;

	// 이 호출 이후에는 Item 포인터를 다시 사용하지 않습니다.
	MarkItemDirty(*Item);
	OnGoldChanged.Broadcast(Gold);

	return Result;
}
