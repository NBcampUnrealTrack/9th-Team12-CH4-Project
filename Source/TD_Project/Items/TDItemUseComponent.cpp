#include "Items/TDItemUseComponent.h"

#include "Combat/TDCombatStatics.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDItemRow.h"
#include "Data/TDItemSetRow.h"
#include "Data/TDItemStatRow.h"
#include "Data/TDItemUseEffectRow.h"
#include "Data/TDOptionRow.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerState.h"
#include "Items/TDInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Stats/TDProgressionComponent.h"
#include "Stats/TDStatComponent.h"

namespace
{
	const TCHAR* ItemUseContext = TEXT("UTDItemUseComponent");
}

UTDItemUseComponent::UTDItemUseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTDItemUseComponent::BeginPlay()
{
	Super::BeginPlay();

	EquippedContainer.OwnerComponent = this;

	// 네 테이블 모두 없어도 크래시는 나지 않지만, 해당 효과가 조용히 빠진다.
	// 어느 것이 빠졌는지 시작할 때 알려준다.
	const TCHAR* MissingTables[] = {
		ItemStatTable == nullptr ? TEXT("ItemStatTable(DT_ItemStat)") : nullptr,
		SetBonusTable == nullptr ? TEXT("SetBonusTable(DT_ItemSetBonus)") : nullptr,
		OptionDefinitionTable == nullptr ? TEXT("OptionDefinitionTable(DT_OptionDefinition)") : nullptr,
		UseEffectTable == nullptr ? TEXT("UseEffectTable(DT_ItemUseEffect)") : nullptr
	};

	for (const TCHAR* Missing : MissingTables)
	{
		if (Missing != nullptr)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("%s: ItemUseComponent 의 %s 가 지정되지 않았다."),
				*GetNameSafe(GetOwner()), Missing);
		}
	}
}

void UTDItemUseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 장착 목록은 본인만 알면 된다. 다른 플레이어에게 보이는 외형은 별도 경로로 다룬다.
	DOREPLIFETIME_CONDITION(UTDItemUseComponent, EquippedContainer, COND_OwnerOnly);
}

bool UTDItemUseComponent::HasAuthorityToModify() const
{
	const AActor* Owner = GetOwner();
	return Owner != nullptr && Owner->HasAuthority();
}

UTDInventoryComponent* UTDItemUseComponent::GetInventory() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UTDInventoryComponent>() : nullptr;
}

UTDStatComponent* UTDItemUseComponent::GetStatComponent() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UTDStatComponent>() : nullptr;
}

UTDProgressionComponent* UTDItemUseComponent::GetProgression() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UTDProgressionComponent>() : nullptr;
}

const FTDItemRow* UTDItemUseComponent::FindItemRow(FName ItemId) const
{
	const UTDInventoryComponent* Inventory = GetInventory();
	return Inventory ? Inventory->FindItemDefinition(ItemId) : nullptr;
}

const FTDItemInstance* UTDItemUseComponent::GetEquipped(int32 EquipSlot) const
{
	return EquippedContainer.Items.FindByPredicate(
		[EquipSlot](const FTDItemInstance& Item) { return Item.SlotIndex == EquipSlot; });
}

FTDItemInstance* UTDItemUseComponent::FindMutableEquipped(int32 EquipSlot)
{
	return EquippedContainer.Items.FindByPredicate(
		[EquipSlot](const FTDItemInstance& Item) { return Item.SlotIndex == EquipSlot; });
}

int32 UTDItemUseComponent::GetSetPieceCount(FName SetId) const
{
	if (SetId.IsNone())
	{
		return 0;
	}

	int32 Count = 0;
	for (const FTDItemInstance& Item : EquippedContainer.Items)
	{
		const FTDItemRow* Row = FindItemRow(Item.ItemId);
		if (Row != nullptr && Row->SetId == SetId)
		{
			++Count;
		}
	}

	return Count;
}

void UTDItemUseComponent::BroadcastEquipmentChanged()
{
	OnEquipmentChanged.Broadcast();
}

bool UTDItemUseComponent::EquipItem(int32 InventorySlot, int32 EquipSlot)
{
	if (!HasAuthorityToModify())
	{
		return false;
	}

	if (EquipSlot < 0 || EquipSlot >= EquipSlotCount)
	{
		return false;
	}

	UTDInventoryComponent* Inventory = GetInventory();
	if (Inventory == nullptr)
	{
		return false;
	}

	const FTDItemInstance* Source = Inventory->FindBySlot(InventorySlot);
	if (Source == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipItem: 인벤토리 %d번 칸이 비어 있다."), InventorySlot);
		return false;
	}

	const FTDItemRow* Row = FindItemRow(Source->ItemId);
	if (Row == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("EquipItem: DT_ItemDefinition 에 '%s' 행이 없다."), *Source->ItemId.ToString());
		return false;
	}

	if (Row->ItemType != TDTags::Item_Type_Accessory.GetTag())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("EquipItem: '%s' 의 ItemType 이 '%s' 다. 장착하려면 Item.Type.Accessory 여야 한다."),
			*Source->ItemId.ToString(),
			Row->ItemType.IsValid() ? *Row->ItemType.ToString() : TEXT("(비어 있음)"));
		return false;
	}

	// 레벨 제한은 획득이 아니라 여기서 걸린다.
	if (Row->RequiredLevel > 0)
	{
		const UTDProgressionComponent* Progression = GetProgression();
		const int32 Level = Progression ? Progression->GetLevel() : 0;
		if (Level < Row->RequiredLevel)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("EquipItem: '%s' 는 %d레벨부터 장착할 수 있다. 현재 %d레벨."),
				*Source->ItemId.ToString(), Row->RequiredLevel, Level);
			return false;
		}
	}

	// 같은 아이템을 두 칸에 끼울 수 없다. 바꿔 끼는 경우(같은 칸)는 예외다.
	for (const FTDItemInstance& Equipped : EquippedContainer.Items)
	{
		if (Equipped.ItemId == Source->ItemId && Equipped.SlotIndex != EquipSlot)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("EquipItem: '%s' 는 이미 %d번 칸에 장착돼 있다. 같은 아이템은 중복 장착할 수 없다."),
				*Source->ItemId.ToString(), Equipped.SlotIndex);
			return false;
		}
	}

	// 이미 끼워져 있던 것이 있으면 벗겨서 되돌려야 한다.
	FTDItemInstance SwappedOut;
	const FTDItemInstance* Existing = GetEquipped(EquipSlot);
	const bool bNeedsSwapBack = (Existing != nullptr);
	if (bNeedsSwapBack)
	{
		SwappedOut = *Existing;
	}

	// 인벤토리에서 먼저 뺀다. 이 칸이 비므로 벗겨진 아이템이 들어갈 자리가 보장된다.
	FTDItemInstance ItemToEquip;
	if (!Inventory->TakeItemAt(InventorySlot, ItemToEquip))
	{
		return false;
	}

	if (bNeedsSwapBack)
	{
		EquippedContainer.Items.RemoveAll(
			[EquipSlot](const FTDItemInstance& Entry) { return Entry.SlotIndex == EquipSlot; });
	}

	ItemToEquip.SlotIndex = EquipSlot;
	FTDItemInstance& Added = EquippedContainer.Items.Add_GetRef(ItemToEquip);
	EquippedContainer.MarkItemDirty(Added);

	// 벗겨진 아이템은 방금 비운 칸으로 돌아간다.
	if (bNeedsSwapBack)
	{
		Inventory->PutItemAt(InventorySlot, SwappedOut);
	}

	EquippedContainer.MarkArrayDirty();
	RefreshEquipmentModifiers();
	BroadcastEquipmentChanged();

	return true;
}

bool UTDItemUseComponent::UnequipItem(int32 EquipSlot)
{
	if (!HasAuthorityToModify())
	{
		return false;
	}

	const FTDItemInstance* Equipped = GetEquipped(EquipSlot);
	if (Equipped == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnequipItem: 장착칸 %d 가 비어 있다."), EquipSlot);
		return false;
	}

	UTDInventoryComponent* Inventory = GetInventory();
	if (Inventory == nullptr)
	{
		return false;
	}

	// 되돌릴 자리가 없으면 벗지 않는다. 벗고 나서 버릴 곳이 없으면 아이템이 사라진다.
	FTDItemInstance ItemToReturn = *Equipped;
	if (!Inventory->PutItemInFirstEmptySlot(ItemToReturn))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UnequipItem: 인벤토리에 빈 칸이 없어 해제하지 않았다. (%d / %d 사용 중)"),
			Inventory->GetUsedSlotCount(), Inventory->GetSlotCapacity());
		return false;
	}

	EquippedContainer.Items.RemoveAll(
		[EquipSlot](const FTDItemInstance& Entry) { return Entry.SlotIndex == EquipSlot; });

	EquippedContainer.MarkArrayDirty();
	RefreshEquipmentModifiers();
	BroadcastEquipmentChanged();

	return true;
}

bool UTDItemUseComponent::UseItem(int32 InventorySlot)
{
	if (!HasAuthorityToModify())
	{
		return false;
	}

	UTDInventoryComponent* Inventory = GetInventory();
	if (Inventory == nullptr || UseEffectTable == nullptr)
	{
		return false;
	}

	const FTDItemInstance* Item = Inventory->FindBySlot(InventorySlot);
	if (Item == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UseItem: 인벤토리 %d번 칸이 비어 있다."), InventorySlot);
		return false;
	}

	const FName ItemId = Item->ItemId;
	const FTDItemRow* Row = FindItemRow(ItemId);
	if (Row == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UseItem: DT_ItemDefinition 에 '%s' 행이 없다."), *ItemId.ToString());
		return false;
	}

	if (Row->ItemType != TDTags::Item_Type_Consumable.GetTag())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UseItem: '%s' 의 ItemType 이 '%s' 다. 사용하려면 Item.Type.Consumable 이어야 한다."),
			*ItemId.ToString(),
			Row->ItemType.IsValid() ? *Row->ItemType.ToString() : TEXT("(비어 있음)"));
		return false;
	}

	if (Row->RequiredLevel > 0)
	{
		const UTDProgressionComponent* Progression = GetProgression();
		const int32 Level = Progression ? Progression->GetLevel() : 0;
		if (Level < Row->RequiredLevel)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("UseItem: '%s' 는 %d레벨부터 쓸 수 있다. 현재 %d레벨."),
				*ItemId.ToString(), Row->RequiredLevel, Level);
			return false;
		}
	}

	// 이 아이템에 걸린 효과를 모두 모은다. 엘릭서처럼 여러 줄일 수 있다.
	TArray<FTDItemUseEffectRow*> EffectRows;
	UseEffectTable->GetAllRows<FTDItemUseEffectRow>(ItemUseContext, EffectRows);

	bool bAppliedAny = false;
	for (const FTDItemUseEffectRow* Effect : EffectRows)
	{
		if (Effect == nullptr || Effect->ItemId != ItemId || !Effect->EffectTag.IsValid())
		{
			continue;
		}

		// 엘릭서처럼 효과가 여럿이면 하나라도 먹히면 소모한다.
		// 체력만 가득 찬 상태에서 체력+마나 포션을 쓰는 경우가 여기 해당한다.
		bAppliedAny |= ApplyUseEffect(Effect->EffectTag, Effect->Value);
	}

	if (!bAppliedAny)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UseItem: '%s' 의 효과가 하나도 적용되지 않아 소비하지 않았다. "
				 "(효과 미정의 / 이미 가득 참 / 캐릭터 없음)"), *ItemId.ToString());
		return false;
	}

	// 효과가 실제로 적용됐을 때만 개수를 줄인다.
	Inventory->ConsumeItemAt(InventorySlot, 1);
	return true;
}

// ── UI 진입점 ─────────────────────────────────────────────
// 전송 코드는 UHT 가 생성한다. 여기서는 실제 처리에 넘기기만 한다.

void UTDItemUseComponent::ServerEquipItem_Implementation(int32 InventorySlot, int32 EquipSlot)
{
	EquipItem(InventorySlot, EquipSlot);
}

void UTDItemUseComponent::ServerUnequipItem_Implementation(int32 EquipSlot)
{
	UnequipItem(EquipSlot);
}

void UTDItemUseComponent::ServerUseItem_Implementation(int32 InventorySlot)
{
	UseItem(InventorySlot);
}

AActor* UTDItemUseComponent::GetOwnerCharacter() const
{
	const APlayerState* PlayerState = Cast<APlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetPawn() : nullptr;
}

bool UTDItemUseComponent::ApplyUseEffect(FGameplayTag EffectTag, float Value)
{
	if (EffectTag == TDTags::Item_Effect_ExpandInventory.GetTag())
	{
		UTDInventoryComponent* Inventory = GetInventory();
		if (Inventory == nullptr)
		{
			return false;
		}

		// 상한에 걸려 한 칸도 못 늘리면 확장권을 쓰지 않은 것으로 본다.
		const int32 Before = Inventory->GetSlotCapacity();
		Inventory->SetSlotCapacity(Before + FMath::RoundToInt(Value));

		if (Inventory->GetSlotCapacity() == Before)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ApplyUseEffect: 인벤토리가 이미 상한(%d칸)이라 확장하지 못했다."), Before);
			return false;
		}

		return true;
	}

	// 회복은 PlayerState 가 아니라 월드의 캐릭터에게 적용한다.
	if (EffectTag == TDTags::Item_Effect_RestoreHealth.GetTag()
		|| EffectTag == TDTags::Item_Effect_RestoreMana.GetTag())
	{
		AActor* Character = GetOwnerCharacter();
		if (Character == nullptr)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ApplyUseEffect: 조종 중인 캐릭터가 없다. 캐릭터를 선택하기 전에는 회복할 수 없다."));
			return false;
		}

		const bool bRestored = (EffectTag == TDTags::Item_Effect_RestoreHealth.GetTag())
			? UTDCombatStatics::RestoreHealth(Character, Value)
			: UTDCombatStatics::RestoreMana(Character, Value);

		if (!bRestored)
		{
			// 이미 가득 찼거나 죽은 상태다. 아이템을 소모하지 않고 돌려준다.
			UE_LOG(LogTemp, Log,
				TEXT("ApplyUseEffect: '%s' 가 적용되지 않았다 (이미 가득 찼거나 사망 상태)."),
				*EffectTag.ToString());
		}

		return bRestored;
	}

	UE_LOG(LogTemp, Warning, TEXT("ApplyUseEffect: 알 수 없는 효과 '%s'."), *EffectTag.ToString());
	return false;
}

void UTDItemUseComponent::RefreshEquipmentModifiers()
{
	UTDStatComponent* StatComponent = GetStatComponent();
	if (StatComponent == nullptr)
	{
		return;
	}

	TArray<FTDStatModifier> Modifiers;

	// ── 1. 각 장신구의 고정 옵션 ──
	if (ItemStatTable != nullptr)
	{
		TArray<FTDItemStatRow*> StatRows;
		ItemStatTable->GetAllRows<FTDItemStatRow>(ItemUseContext, StatRows);

		for (const FTDItemInstance& Item : EquippedContainer.Items)
		{
			for (const FTDItemStatRow* StatRow : StatRows)
			{
				if (StatRow == nullptr || StatRow->ItemId != Item.ItemId || !StatRow->StatTag.IsValid())
				{
					continue;
				}

				Modifiers.Emplace(StatRow->StatTag, StatRow->Op, StatRow->Value);
			}
		}
	}

	// ── 2. 굴려진 추가 옵션 ──
	if (OptionDefinitionTable != nullptr)
	{
		for (const FTDItemInstance& Item : EquippedContainer.Items)
		{
			for (const FTDItemOption& Option : Item.Options)
			{
				const FTDOptionDefinitionRow* OptionRow =
					OptionDefinitionTable->FindRow<FTDOptionDefinitionRow>(Option.OptionId, ItemUseContext, false);

				if (OptionRow == nullptr || !OptionRow->StatTag.IsValid())
				{
					continue;
				}

				// 값은 굴릴 때 정해졌다. 여기서는 어떤 스탯인지만 정의에서 읽는다.
				Modifiers.Emplace(OptionRow->StatTag, OptionRow->Op, Option.Value);
			}
		}
	}

	// ── 3. 세트 단계 효과 ──
	// 효과는 누적이므로 조건을 만족하는 단계를 전부 넣는다.
	if (SetBonusTable != nullptr)
	{
		TMap<FName, int32> SetCounts;
		for (const FTDItemInstance& Item : EquippedContainer.Items)
		{
			const FTDItemRow* Row = FindItemRow(Item.ItemId);
			if (Row != nullptr && !Row->SetId.IsNone())
			{
				++SetCounts.FindOrAdd(Row->SetId);
			}
		}

		if (SetCounts.Num() > 0)
		{
			TArray<FTDItemSetBonusRow*> BonusRows;
			SetBonusTable->GetAllRows<FTDItemSetBonusRow>(ItemUseContext, BonusRows);

			for (const FTDItemSetBonusRow* Bonus : BonusRows)
			{
				if (Bonus == nullptr || !Bonus->StatTag.IsValid())
				{
					continue;
				}

				const int32* Count = SetCounts.Find(Bonus->SetId);
				if (Count == nullptr || *Count < Bonus->RequiredCount)
				{
					continue;
				}

				Modifiers.Emplace(Bonus->StatTag, Bonus->Op, Bonus->Value);
			}
		}
	}

	// 장착 전체를 소스 하나로 갈아 끼운다. 반지 하나만 빼도 세트 단계가 바뀌므로
	// 부분 갱신이 불가능하고, 어차피 전부 다시 만드는 편이 정확하다.
	//
	// 제거와 등록을 따로 부르면 그 사이의 "장비 효과가 없는 순간" 이 알림으로 새어 나간다.
	EquipmentSourceHandle = StatComponent->ReplaceSource(
		EquipmentSourceHandle, TDTags::Source_Equipment, MoveTemp(Modifiers));
}

void UTDItemUseComponent::WriteSaveData(FTDPlayerSaveData& Out) const
{
	Out.EquippedItems = EquippedContainer.Items;
}

void UTDItemUseComponent::ReadSaveData(const FTDPlayerSaveData& In)
{
	EquippedContainer.Items = In.EquippedItems;
	EquippedContainer.MarkArrayDirty();

	RefreshEquipmentModifiers();
	BroadcastEquipmentChanged();
}
