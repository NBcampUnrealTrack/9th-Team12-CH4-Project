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
#include "Items/TDEnhanceStatics.h"
#include "Items/TDInventoryComponent.h"
#include "Items/TDItemOptionStatics.h"
#include "Net/UnrealNetwork.h"
#include "Settings/TDItemOptionSettings.h"
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

	// 경험치 계열. 둘 다 결국 AddExp 로 흘러가고, 얼마를 줄지 정하는 방식만 다르다.
	if (EffectTag == TDTags::Item_Effect_GainExp.GetTag()
		|| EffectTag == TDTags::Item_Effect_LevelUp.GetTag())
	{
		UTDProgressionComponent* Progression = GetProgression();
		if (Progression == nullptr)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ApplyUseEffect: 성장 컴포넌트가 없어 경험치를 줄 수 없다."));
			return false;
		}

		int32 ExpToGive = 0;

		if (EffectTag == TDTags::Item_Effect_GainExp.GetTag())
		{
			ExpToGive = FMath::RoundToInt(Value);
		}
		else
		{
			// 레벨업권. Value 는 경험치가 아니라 **상한 레벨**이다.
			//
			// 상한 미만이면 다음 레벨까지 필요한 만큼을 줘서 확실히 한 칸 올린다.
			// 상한 이상이면 상한 레벨 한 구간만큼의 고정량만 준다 — 그러지 않으면
			// 만렙 직전에 쓸수록 이득이 커져 무한 레벨업권이 된다.
			const int32 CapLevel = FMath::RoundToInt(Value);

			ExpToGive = (Progression->GetLevel() < CapLevel)
				? Progression->GetExpToNextLevel()
				: Progression->GetExpSpanForLevel(CapLevel);
		}

		if (ExpToGive <= 0)
		{
			// 만렙이거나 곡선에 없는 레벨을 상한으로 지정한 경우다. 아이템을 소모하지 않는다.
			UE_LOG(LogTemp, Log,
				TEXT("ApplyUseEffect: '%s' 로 줄 경험치가 없다 (만렙이거나 상한 레벨이 곡선 밖)."),
				*EffectTag.ToString());
			return false;
		}

		Progression->AddExp(ExpToGive);
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("ApplyUseEffect: 알 수 없는 효과 '%s'."), *EffectTag.ToString());
	return false;
}

float UTDItemUseComponent::GetEnhanceMultiplier(FName ItemId, int32 EnhanceLevel) const
{
	if (EnhanceLevel <= 0)
	{
		return 1.f;
	}

	// 아이템 정의를 못 찾으면 RequiredLevel 0 으로 본다 — 가장 낮은 배율(2%/강)이라
	// 데이터가 빠졌을 때 스탯이 부풀어 오르는 쪽으로 틀리지 않는다.
	const FTDItemRow* ItemRow = FindItemRow(ItemId);
	const int32 RequiredLevel = ItemRow != nullptr ? ItemRow->RequiredLevel : 0;

	return TDEnhance::GetStatMultiplier(EnhanceLevel, RequiredLevel);
}

void UTDItemUseComponent::RefreshEquipmentModifiers()
{
	UTDStatComponent* StatComponent = GetStatComponent();
	if (StatComponent == nullptr)
	{
		return;
	}

	TArray<FTDStatModifier> Modifiers;

	// ── 1. 각 장신구의 고정 옵션 (+ 강화 배율) ──
	if (ItemStatTable != nullptr)
	{
		TArray<FTDItemStatRow*> StatRows;
		ItemStatTable->GetAllRows<FTDItemStatRow>(ItemUseContext, StatRows);

		for (const FTDItemInstance& Item : EquippedContainer.Items)
		{
			// 강화 배율은 이 아이템의 착용 레벨제한에 따라 정해진다(TDEnhanceStatics).
			// EnhanceLevel 이 0 이면 1.0 이라 강화 전과 완전히 같은 값이 나온다.
			const float EnhanceMultiplier = GetEnhanceMultiplier(Item.ItemId, Item.EnhanceLevel);

			for (const FTDItemStatRow* StatRow : StatRows)
			{
				if (StatRow == nullptr || StatRow->ItemId != Item.ItemId || !StatRow->StatTag.IsValid())
				{
					continue;
				}

				Modifiers.Emplace(StatRow->StatTag, StatRow->Op, StatRow->Value * EnhanceMultiplier);
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

// ── 추가 옵션 (잠재능력) ──────────────────────────────────

ETDRerollResult UTDItemUseComponent::PrepareReroll(int32 SlotIndex, bool bEquipped,
	const FTDItemInstance*& OutItem, const FTDItemRow*& OutDefinition,
	TArray<FTDOptionRarityRow>& OutSortedRarities, int32& OutCost) const
{
	OutItem = nullptr;
	OutDefinition = nullptr;
	OutCost = 0;

	const UTDItemOptionSettings* Settings = UTDItemOptionSettings::Get();
	if (Settings == nullptr || OptionDefinitionTable == nullptr)
	{
		return ETDRerollResult::InternalError;
	}

	// 대상을 찾는다. 장착 아이템은 인벤토리에 없으므로 칸 번호만으로는 구분할 수 없다.
	if (bEquipped)
	{
		OutItem = GetEquipped(SlotIndex);
	}
	else
	{
		const APlayerState* OwnerState = Cast<APlayerState>(GetOwner());
		const UTDInventoryComponent* Inventory =
			OwnerState ? OwnerState->FindComponentByClass<UTDInventoryComponent>() : nullptr;

		OutItem = Inventory ? Inventory->FindBySlot(SlotIndex) : nullptr;
	}

	if (OutItem == nullptr)
	{
		return ETDRerollResult::ItemNotFound;
	}

	OutDefinition = FindItemRow(OutItem->ItemId);
	if (OutDefinition == nullptr)
	{
		return ETDRerollResult::InternalError;
	}

	// 추가 옵션은 장신구만 가진다. 소비 아이템에 붙여봐야 쓰는 순간 사라진다.
	if (OutDefinition->ItemType != TDTags::Item_Type_Accessory.GetTag())
	{
		return ETDRerollResult::NotAccessory;
	}

	if (OutDefinition->OptionPoolId.IsNone())
	{
		// 장신구인데 풀이 지정되지 않았다. 데이터 누락이라 플레이어 잘못이 아니다.
		return ETDRerollResult::InternalError;
	}

	const UDataTable* RarityTable = Settings->OptionRarityTable.LoadSynchronous();
	OutSortedRarities = TDItemOption::GetSortedRarities(RarityTable);

	if (OutSortedRarities.Num() == 0)
	{
		return ETDRerollResult::InternalError;
	}

	// 아직 한 번도 안 굴린 아이템은 등급이 비어 있다. 가장 낮은 등급에서 시작한다 —
	// DT_ItemDefinition 의 InitialOptionRarity 가 있으면 그쪽이 이긴다.
	FGameplayTag CurrentRarity = OutItem->OptionRarity;
	if (!CurrentRarity.IsValid())
	{
		CurrentRarity = OutDefinition->InitialOptionRarity.IsValid()
			? OutDefinition->InitialOptionRarity
			: OutSortedRarities[0].Rarity;
	}

	const FTDOptionRarityRow* RarityRow = TDItemOption::FindRarity(OutSortedRarities, CurrentRarity);
	if (RarityRow == nullptr)
	{
		return ETDRerollResult::InternalError;
	}

	OutCost = TDItemOption::GetRerollCost(RarityRow->RerollCost, OutDefinition->RequiredLevel);
	return ETDRerollResult::Success;
}

int32 UTDItemUseComponent::GetRerollCost(int32 SlotIndex, bool bEquipped) const
{
	const FTDItemInstance* Item = nullptr;
	const FTDItemRow* Definition = nullptr;
	TArray<FTDOptionRarityRow> Sorted;
	int32 Cost = 0;

	// 실패해도 0 을 돌려주면 된다. UI 는 "굴릴 수 없다" 를 CanReroll 이 아니라
	// 비용이 0 인지로 판단해도 무방하다.
	PrepareReroll(SlotIndex, bEquipped, Item, Definition, Sorted, Cost);
	return Cost;
}

void UTDItemUseComponent::ServerRerollOptions_Implementation(int32 SlotIndex, bool bEquipped)
{
	// 치트와 기존 호출부의 입구. 결과는 Client RPC 로만 돌아간다.
	FGameplayTag Rarity;
	const ETDRerollResult Result = RerollOptionsForService(SlotIndex, bEquipped, Rarity);

	ClientOptionsRerolled(Result, Rarity);
}

ETDRerollResult UTDItemUseComponent::RerollOptionsForService(
	int32 SlotIndex, bool bEquipped, FGameplayTag& OutRarity)
{
	OutRarity = FGameplayTag();

	const APlayerState* OwnerState = Cast<APlayerState>(GetOwner());
	UTDInventoryComponent* Inventory =
		OwnerState ? OwnerState->FindComponentByClass<UTDInventoryComponent>() : nullptr;

	if (Inventory == nullptr)
	{
		return ETDRerollResult::InternalError;
	}

	const FTDItemInstance* Item = nullptr;
	const FTDItemRow* Definition = nullptr;
	TArray<FTDOptionRarityRow> Sorted;
	int32 Cost = 0;

	const ETDRerollResult Prepared =
		PrepareReroll(SlotIndex, bEquipped, Item, Definition, Sorted, Cost);

	if (Prepared != ETDRerollResult::Success)
	{
		return Prepared;
	}

	if (!Inventory->SpendGold(Cost))
	{
		OutRarity = Item->OptionRarity;
		return ETDRerollResult::NotEnoughGold;
	}

	// 아직 등급이 없으면 여기서 정해진다(PrepareReroll 과 같은 규칙).
	FGameplayTag Rarity = Item->OptionRarity;
	if (!Rarity.IsValid())
	{
		Rarity = Definition->InitialOptionRarity.IsValid()
			? Definition->InitialOptionRarity
			: Sorted[0].Rarity;
	}

	// ── 승급 판정 ──
	// 굴릴 때마다 낮은 확률로 한 단계 오른다. 이것이 재굴림의 목표다(D31) —
	// 없으면 수치만 반복해 뽑는 일이 된다.
	const FTDOptionRarityRow* RarityRow = TDItemOption::FindRarity(Sorted, Rarity);
	bool bUpgraded = false;

	if (RarityRow != nullptr && FMath::FRand() < RarityRow->UpgradeChance)
	{
		const FGameplayTag Next = TDItemOption::GetNextRarity(Sorted, Rarity);
		bUpgraded = (Next != Rarity);   // 최고 등급이면 자기 자신이 돌아온다
		Rarity = Next;
	}

	const UTDItemOptionSettings* Settings = UTDItemOptionSettings::Get();
	const int32 LineCount = Settings->OptionLineCount;

	// 줄마다 세 개씩 미리 굴린다 — [등급판정, 옵션선택, 수치].
	// 계산 함수를 순수하게 두려고 주사위를 밖에서 넣는다(TDEnhance 와 같은 이유).
	TArray<float> Rolls;
	Rolls.Reserve(LineCount * 3);
	for (int32 Index = 0; Index < LineCount * 3; ++Index)
	{
		Rolls.Add(FMath::FRand());
	}

	const TArray<FTDItemOption> NewOptions = TDItemOption::RollLines(
		Settings->OptionPoolTable.LoadSynchronous(), OptionDefinitionTable,
		Sorted, Definition->OptionPoolId, Rarity, LineCount, Rolls);

	// 적어 넣고 복제시킨다. 컨테이너가 다르므로 경로도 갈린다.
	if (bEquipped)
	{
		// 장착 배열은 내 것이라 직접 고친다. 칸 번호로 다시 찾는 이유는 PrepareReroll 이
		// const 포인터만 돌려주기 때문이다 — 조회와 수정을 섞지 않으려는 것이다.
		FTDItemInstance* Mutable = EquippedContainer.Items.FindByPredicate(
			[SlotIndex](const FTDItemInstance& Entry) { return Entry.SlotIndex == SlotIndex; });

		if (Mutable == nullptr)
		{
			OutRarity = Rarity;
			return ETDRerollResult::InternalError;
		}

		Mutable->OptionRarity = Rarity;
		Mutable->Options = NewOptions;
		EquippedContainer.MarkItemDirty(*Mutable);

		// 착용 중이면 스탯을 다시 등록해야 한다. RefreshEquipmentModifiers 가
		// Options 를 읽어 모디파이어로 만드므로 그것만 부르면 반영된다.
		RefreshEquipmentModifiers();
		BroadcastEquipmentChanged();
	}
	else if (!Inventory->SetItemOptions(SlotIndex, Rarity, NewOptions))
	{
		OutRarity = Rarity;
		return ETDRerollResult::InternalError;
	}

	UE_LOG(LogTemp, Log, TEXT("재굴림: %s 칸 %d '%s' — 등급 %s%s, 옵션 %d줄, 비용 %d"),
		bEquipped ? TEXT("장착") : TEXT("인벤"), SlotIndex, *Definition->DisplayName.ToString(),
		*Rarity.ToString(), bUpgraded ? TEXT(" (상승!)") : TEXT(""),
		NewOptions.Num(), Cost);

	OutRarity = Rarity;
	return bUpgraded ? ETDRerollResult::SuccessUpgraded : ETDRerollResult::Success;
}

void UTDItemUseComponent::RollInitialOptions(FTDItemInstance& Item) const
{
	const FTDItemRow* Definition = FindItemRow(Item.ItemId);

	// 추가 옵션은 장신구만 가진다. 포션처럼 풀이 비어 있는 것은 여기서 빠진다.
	if (Definition == nullptr
		|| Definition->ItemType != TDTags::Item_Type_Accessory.GetTag()
		|| Definition->OptionPoolId.IsNone())
	{
		return;
	}

	const UTDItemOptionSettings* Settings = UTDItemOptionSettings::Get();
	if (Settings == nullptr || OptionDefinitionTable == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("RollInitialOptions: 옵션 테이블이 지정되지 않아 '%s' 가 옵션 없이 들어간다."),
			*Item.ItemId.ToString());
		return;
	}

	const TArray<FTDOptionRarityRow> Sorted =
		TDItemOption::GetSortedRarities(Settings->OptionRarityTable.LoadSynchronous());

	if (Sorted.Num() == 0)
	{
		// 조용히 빠져나가면 "장신구가 옵션 없이 들어온다" 만 보이고 원인이 로그에 안 남는다.
		// dev 머지로 ini 의 이 섹션이 통째로 사라졌을 때 실제로 그랬다.
		UE_LOG(LogTemp, Warning,
			TEXT("RollInitialOptions: 옵션 등급표를 읽지 못해 '%s' 가 옵션 없이 들어간다. ")
			TEXT("프로젝트 세팅 > TD > Item Option 의 OptionRarityTable 을 확인할 것."),
			*Item.ItemId.ToString());
		return;
	}

	// PrepareReroll · ServerRerollOptions 의 "아직 등급이 없으면" 과 같은 규칙이다.
	const FGameplayTag Rarity = Definition->InitialOptionRarity.IsValid()
		? Definition->InitialOptionRarity
		: Sorted[0].Rarity;

	const int32 LineCount = Settings->OptionLineCount;

	TArray<float> Rolls;
	Rolls.Reserve(LineCount * 3);
	for (int32 Index = 0; Index < LineCount * 3; ++Index)
	{
		Rolls.Add(FMath::FRand());
	}

	Item.OptionRarity = Rarity;
	Item.Options = TDItemOption::RollLines(
		Settings->OptionPoolTable.LoadSynchronous(), OptionDefinitionTable,
		Sorted, Definition->OptionPoolId, Rarity, LineCount, Rolls);
}

void UTDItemUseComponent::ClientOptionsRerolled_Implementation(
	ETDRerollResult Result, FGameplayTag NewRarity)
{
	// 문구는 만들지 않는다. UI 가 이 델리게이트를 받아 자기 형식으로 표시한다.
	OnOptionsRerolled.Broadcast(Result, NewRarity);
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
