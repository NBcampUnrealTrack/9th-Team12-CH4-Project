#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Character/TDCharacterBase.h"
#include "Core/TDGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Blueprint/UserWidget.h"
#include "Chat/TDChatFilter.h"
#include "Data/TDBannedWordRow.h"
#include "Engine/GameInstance.h"
#include "Items/TDEnhanceStatics.h"
#include "Market/TDMarketSubsystem.h"
#include "Items/TDInventoryComponent.h"
#include "Items/TDQuickSlotComponent.h"
#include "Party/TDPartyComponent.h"
#include "Settings/TDChatSettings.h"
#include "Settings/TDInputSettingsLibrary.h"
#include "UI/Common/Tooltip/TDTooltipStatics.h"
#include "UI/Core/TDUIManagerSubsystem.h"
#include "UI/HUD/Nav/TDNavMenuTypes.h"
#include "UI/Settings/TDUISettings.h"
#include "Items/TDItemUseComponent.h"
#include "Character/TDPlayerCharacter.h"
#include "Shop/TDShopStatics.h"
#include "Player/TDPlayerController.h"
#include "Player/TDPlayerState.h"
#include "Skill/TDSkillComponent.h"
#include "Stats/TDProgressionComponent.h"
#include "Stats/TDStatComponent.h"
#include "Combat/TDCombatStatics.h"
#include "Combat/TDCombatComponent.h"

/**
 * 개발용 콘솔 명령.
 *
 * 클래스가 아니라 전역으로 등록한다. GameMode·PlayerController·Pawn 어디에도 매달리지 않으므로
 * 레벨에 몬스터만 배치한 상태에서도 쓸 수 있다.
 *
 * 다만 로컬에서만 실행된다. 스탯 컴포넌트는 복제되지 않아 클라이언트는 값을 모르므로,
 * 2인 PIE 의 클라이언트 창에서 치면 의미가 없다. 단일 PIE 나 서버 창에서 쓸 것.
 * GameMode 와 PlayerController 가 생기면 CheatManager 로 옮기고 Server RPC 를 태우면 된다.
 */

#if !UE_BUILD_SHIPPING

DEFINE_LOG_CATEGORY_STATIC(LogTDDebug, Log, All);

/** TD.InputDebug 가 읽고 쓰는 값. 캐릭터의 Move 핸들러가 이걸 보고 로그를 찍는다. */
static int32 GTDInputDebugValue = 0;

namespace TDDebugCommands
{
	/**
	 * 클라이언트라면 로컬 컨트롤러를 돌려준다. 서버면 nullptr.
	 *
	 * 콘솔 명령은 로컬에서만 실행되므로, 클라이언트에서 상태를 바꾸려면
	 * 컨트롤러의 Server RPC 를 태워야 한다. 이때는 자기 캐릭터만 대상이 되어
	 * 이름 필터가 의미를 잃는다.
	 */
	static ATDPlayerController* GetClientControllerForCheat(UWorld* World)
	{
		if (World == nullptr || World->GetNetMode() != NM_Client)
		{
			return nullptr;
		}

		return Cast<ATDPlayerController>(World->GetFirstPlayerController());
	}

	/** 출력할 스탯 목록. DT_StatDefinition 을 읽지 않고 여기 나열해서 컴포넌트 내부에 손대지 않는다. */
	static TArray<FGameplayTag> GetDisplayStats()
	{
		return {
			TDTags::Stat_Resource_Health_Max.GetTag(),
			TDTags::Stat_Resource_Health_Regen.GetTag(),
			TDTags::Stat_Resource_Mana_Max.GetTag(),
			TDTags::Stat_Resource_Mana_Regen.GetTag(),
			TDTags::Stat_Offense_Damage.GetTag(),
			TDTags::Stat_Offense_Damage_Physical.GetTag(),
			TDTags::Stat_Offense_Damage_Magical.GetTag(),
			TDTags::Stat_Offense_CritChance.GetTag(),
			TDTags::Stat_Offense_CritDamage.GetTag(),
			TDTags::Stat_Offense_ArmorPenetration.GetTag(),
			TDTags::Stat_Offense_BossDamage.GetTag(),
			TDTags::Stat_Defense_Armor.GetTag(),
			TDTags::Stat_Defense_DamageReduction.GetTag(),
			TDTags::Stat_Utility_MoveSpeed.GetTag(),
			TDTags::Stat_Utility_CooldownRecoveryRate.GetTag()
		};
	}

	/**
	 * 조건에 맞는 캐릭터를 순회한다.
	 * NameFilter 가 비어 있으면 전부, 아니면 액터 이름에 그 문자열이 든 것만.
	 */
	static int32 ForEachCharacter(UWorld* World, const FString& NameFilter,
		TFunctionRef<void(ATDCharacterBase&)> Visitor)
	{
		if (World == nullptr)
		{
			return 0;
		}

		int32 VisitedCount = 0;
		for (TActorIterator<ATDCharacterBase> It(World); It; ++It)
		{
			ATDCharacterBase* Character = *It;
			if (Character == nullptr)
			{
				continue;
			}

			if (!NameFilter.IsEmpty() && !Character->GetName().Contains(NameFilter))
			{
				continue;
			}

			Visitor(*Character);
			++VisitedCount;
		}

		if (VisitedCount == 0)
		{
			// 캐릭터 선택 전에는 Pawn 이 아예 없다(D54). 버그로 오해하기 쉬우므로 다음 단계를 알려준다.
			UE_LOG(LogTDDebug, Warning,
				TEXT("대상 캐릭터를 찾지 못했다. (필터: '%s')\n"
					 "    캐릭터를 고르기 전에는 Pawn 이 스폰되지 않는다.\n"
					 "    TD.GiveTestCharacters → TD.SelectCharacter <번호> 순서로 진행할 것."),
				*NameFilter);
		}

		return VisitedCount;
	}

	static void DumpStats(const TArray<FString>& Args, UWorld* World)
	{
		const FString NameFilter = Args.IsValidIndex(0) ? Args[0] : FString();
		const TArray<FGameplayTag> DisplayStats = GetDisplayStats();

		ForEachCharacter(World, NameFilter, [&DisplayStats](ATDCharacterBase& Character)
		{
			const UTDStatComponent* StatComponent = Character.GetStatComponent();
			if (StatComponent == nullptr)
			{
				UE_LOG(LogTDDebug, Warning, TEXT("%s — 스탯 컴포넌트를 찾을 수 없다."), *Character.GetName());
				return;
			}

			const UTDProgressionComponent* Progression = Character.GetProgressionComponent();
			if (Progression != nullptr)
			{
				UE_LOG(LogTDDebug, Log, TEXT("%s  (Level %d, Class '%s', 잔여 스킬포인트 %d)"),
					*Character.GetName(),
					Progression->GetLevel(),
					*Progression->GetClassId().ToString(),
					Progression->GetRemainingSkillPoints());
			}
			else
			{
				// 몬스터는 성장 컴포넌트가 없다. 정상이다.
				UE_LOG(LogTDDebug, Log, TEXT("%s  (성장 컴포넌트 없음)"), *Character.GetName());
			}

			// 로컬 계산과 복제된 값을 나란히 찍는다. 서버에서는 같지만 클라이언트에서는
			// 로컬이 테이블 기본값이라 크게 차이 난다 — 스탯창이 어느 쪽을 써야 하는지 보여준다.
			const ATDPlayerState* StatOwner = Cast<ATDPlayerState>(Character.GetPlayerState());

			for (const FGameplayTag& Stat : DisplayStats)
			{
				if (StatOwner != nullptr)
				{
					UE_LOG(LogTDDebug, Log, TEXT("    %-40s %8.3f  (복제 %.3f)"),
						*Stat.ToString(), StatComponent->GetStat(Stat), StatOwner->GetReplicatedStat(Stat));
				}
				else
				{
					// 몬스터는 PlayerState 가 없다. 스탯 복제도 필요 없다.
					UE_LOG(LogTDDebug, Log, TEXT("    %-40s %8.3f"),
						*Stat.ToString(), StatComponent->GetStat(Stat));
				}
			}

			// 둘을 나란히 찍는다. 서버에서는 같은 값이지만, 클라이언트에서는
			// 로컬 계산이 기본값 기준이라 복제된 값과 크게 차이 난다.
			// UI 가 어느 쪽을 써야 하는지 헷갈릴 때 이 출력이 근거가 된다.
			UE_LOG(LogTDDebug, Log, TEXT("    %-40s %.0f"),
				TEXT("전투력 (로컬 계산)"), StatComponent->GetCombatPower());

			if (const ATDPlayerState* TDPlayerState = Cast<ATDPlayerState>(Character.GetPlayerState()))
			{
				UE_LOG(LogTDDebug, Log, TEXT("    %-40s %d"),
					TEXT("전투력 (복제됨)"), TDPlayerState->GetCombatPower());
			}

			// 어트리뷰트는 복제되므로 클라이언트에서도 실제 값이 나온다.
			// 스탯이 기본값으로만 보이는 것과 대비된다 — 이게 S5 연결이 동작하는지 보는 지표다.
			if (const UAbilitySystemComponent* ASC = Character.GetAbilitySystemComponent())
			{
				UE_LOG(LogTDDebug, Log, TEXT("    %-40s %.0f / %.0f"),
					TEXT("체력 (어트리뷰트)"),
					ASC->GetNumericAttribute(UTDAttributeSet::GetHealthAttribute()),
					ASC->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute()));

				UE_LOG(LogTDDebug, Log, TEXT("    %-40s %.0f / %.0f"),
					TEXT("마나 (어트리뷰트)"),
					ASC->GetNumericAttribute(UTDAttributeSet::GetManaAttribute()),
					ASC->GetNumericAttribute(UTDAttributeSet::GetMaxManaAttribute()));
			}
			else
			{
				UE_LOG(LogTDDebug, Warning, TEXT("    ASC 를 찾을 수 없다 (InitAbilityActorInfo 확인)"));
			}
		});
	}

	static void SetLevel(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.SetLevel <레벨> [이름필터]"));
			return;
		}

		const int32 NewLevel = FCString::Atoi(*Args[0]);
		const FString NameFilter = Args.IsValidIndex(1) ? Args[1] : FString();

		if (ATDPlayerController* ClientController = GetClientControllerForCheat(World))
		{
			ClientController->ServerDebugSetLevel(NewLevel);
			UE_LOG(LogTDDebug, Log, TEXT("서버에 레벨 %d 설정을 요청했다. (자기 캐릭터만)"), NewLevel);
			return;
		}

		ForEachCharacter(World, NameFilter, [NewLevel](ATDCharacterBase& Character)
		{
			UTDProgressionComponent* Progression = Character.GetProgressionComponent();
			if (Progression == nullptr)
			{
				return;
			}

			// 반환값을 그대로 보고한다. 실패했는데 성공으로 찍히면 디버깅을 방해한다.
			const bool bChanged = Progression->SetLevel(NewLevel);
			UE_LOG(LogTDDebug, Log, TEXT("%s — 레벨 %d 설정 %s"),
				*Character.GetName(), NewLevel, bChanged ? TEXT("성공") : TEXT("실패"));
		});
	}

	/** 인벤토리 컴포넌트를 찾는다. PlayerState 에 붙어 있으므로 캐릭터에서 한 단계 건너가야 한다. */
	static UTDInventoryComponent* FindInventory(ATDCharacterBase& Character)
	{
		const APlayerState* PlayerState = Character.GetPlayerState();
		return PlayerState ? PlayerState->FindComponentByClass<UTDInventoryComponent>() : nullptr;
	}

	static void GiveItem(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.GiveItem <ItemId> [개수] [이름필터]"));
			return;
		}

		const FName ItemId(*Args[0]);
		const int32 Count = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 1;
		const FString NameFilter = Args.IsValidIndex(2) ? Args[2] : FString();

		// 콘솔은 공백으로 인자를 나눈다. 이름에 공백이 있으면 뒷부분이 개수로 해석되어
		// Atoi 가 0을 돌려준다. RowName 에 공백을 쓰지 않는 편이 낫다.
		if (Count <= 0)
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("개수가 %d 다. 아이템 이름에 공백이 있으면 콘솔이 인자를 나눠버린다. "
					 "RowName 을 그대로 한 덩어리로 입력할 것."), Count);
			return;
		}

		if (ATDPlayerController* ClientController = GetClientControllerForCheat(World))
		{
			ClientController->ServerDebugGiveItem(ItemId, Count);
			UE_LOG(LogTDDebug, Log,
				TEXT("서버에 '%s' %d개 지급을 요청했다. (자기 캐릭터만 / 결과는 서버 로그에)"),
				*ItemId.ToString(), Count);
			return;
		}

		ForEachCharacter(World, NameFilter, [ItemId, Count](ATDCharacterBase& Character)
		{
			UTDInventoryComponent* Inventory = FindInventory(Character);
			if (Inventory == nullptr)
			{
				return;
			}

			const bool bAdded = Inventory->AddItem(ItemId, Count);
			UE_LOG(LogTDDebug, Log, TEXT("%s — '%s' %d개 지급 %s"),
				*Character.GetName(), *ItemId.ToString(), Count,
				bAdded ? TEXT("성공") : TEXT("실패 (사유는 위 경고 참조)"));
		});
	}

	/**
	 * 테스트에 자주 쓰는 소비 아이템을 한 번에 넣는다.
	 *
	 * TD.GiveItem 을 네 번 치는 수고를 줄이는 것이 전부라, 지급 자체는 그쪽에 맡긴다 —
	 * 클라이언트에서 치면 Server RPC 를 타고 서버에서 치면 직접 넣는 분기가 이미 거기 있다.
	 *
	 * 목록을 늘리려면 아래 배열에 RowName 을 추가하면 된다. DT_ItemDefinition 에 없는
	 * 이름을 넣으면 그 줄만 실패로 찍히고 나머지는 정상으로 들어간다.
	 */
	static void GiveItemTest(const TArray<FString>& Args, UWorld* World)
	{
		static const TCHAR* TestItemIds[] =
		{
			TEXT("HPotion_Low"),
			TEXT("InvExpand"),
			TEXT("ExpPotion_Low"),
			TEXT("LevelTicket_Low"),
		};

		const int32 Count = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 20;
		if (Count <= 0)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.GiveItemTest [개수=20]"));
			return;
		}

		const FString CountArg = FString::FromInt(Count);
		for (const TCHAR* ItemId : TestItemIds)
		{
			GiveItem({ ItemId, CountArg }, World);
		}
	}

	static void DumpInventory(const TArray<FString>& Args, UWorld* World)
	{
		const FString NameFilter = Args.IsValidIndex(0) ? Args[0] : FString();

		ForEachCharacter(World, NameFilter, [](ATDCharacterBase& Character)
		{
			const UTDInventoryComponent* Inventory = FindInventory(Character);
			if (Inventory == nullptr)
			{
				return;
			}

			UE_LOG(LogTDDebug, Log, TEXT("%s  (%d / %d 칸 사용, %d 골드)"),
				*Character.GetName(), Inventory->GetUsedSlotCount(), Inventory->GetSlotCapacity(),
				Inventory->GetGold());

			// FastArray 는 배열 순서를 보장하지 않는다. 실제 화면은 SlotIndex 로 그리므로
			// 읽는 사람이 헷갈리지 않도록 출력도 슬롯 순으로 맞춘다.
			TArray<FTDItemInstance> SortedItems = Inventory->GetItems();
			SortedItems.Sort([](const FTDItemInstance& A, const FTDItemInstance& B)
			{
				return A.SlotIndex < B.SlotIndex;
			});

			// 강화는 인벤토리 아이템만 대상이라(D76) 결과도 여기서 바로 보여야 한다.
			// 장착까지 해야 확인할 수 있으면 굴린 직후에 맞는지 알 방법이 없다.
			const APlayerController* PC = Cast<APlayerController>(Character.GetController());

			for (const FTDItemInstance& Item : SortedItems)
			{
				UE_LOG(LogTDDebug, Log, TEXT("    [%3d] %-24s x%-4d  강화 +%d  옵션 %d개"),
					Item.SlotIndex, *Item.ItemId.ToString(), Item.Count,
					Item.EnhanceLevel, Item.Options.Num());

				// 강화하지 않은 물건까지 스탯을 늘어놓으면 인벤토리 덤프가 읽기 어려워진다.
				if (Item.EnhanceLevel <= 0)
				{
					continue;
				}

				for (const FTDTooltipLine& Line :
					UTDTooltipStatics::MakeItemStatLines(PC, Item.ItemId, Item.EnhanceLevel))
				{
					UE_LOG(LogTDDebug, Log, TEXT("          %-20s %s"),
						*Line.Label.ToString(), *Line.Value.ToString());
				}
			}
		});
	}

	static UTDItemUseComponent* FindItemUse(ATDCharacterBase& Character)
	{
		const APlayerState* PlayerState = Character.GetPlayerState();
		return PlayerState ? PlayerState->FindComponentByClass<UTDItemUseComponent>() : nullptr;
	}

	static void EquipItem(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(1))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.Equip <인벤슬롯> <장착칸 0~5> [이름필터]"));
			return;
		}

		const int32 InventorySlot = FCString::Atoi(*Args[0]);
		const int32 EquipSlot = FCString::Atoi(*Args[1]);
		const FString NameFilter = Args.IsValidIndex(2) ? Args[2] : FString();

		ForEachCharacter(World, NameFilter, [InventorySlot, EquipSlot](ATDCharacterBase& Character)
		{
			UTDItemUseComponent* ItemUse = FindItemUse(Character);
			if (ItemUse == nullptr)
			{
				return;
			}

			// Server RPC 를 태운다. 서버에서 부르면 네트워크를 타지 않고 즉시 실행되므로
			// 서버·클라 어느 쪽에서 쳐도 같은 코드로 동작한다.
			// 다만 비동기라 결과를 즉시 받을 수 없어, 실패 사유는 서버 로그에 남는다.
			ItemUse->ServerEquipItem(InventorySlot, EquipSlot);
			UE_LOG(LogTDDebug, Log, TEXT("%s — 인벤 %d → 장착칸 %d 요청 전송 (결과는 TD.DumpEquipment 로 확인)"),
				*Character.GetName(), InventorySlot, EquipSlot);
		});
	}

	static void UnequipItem(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.Unequip <장착칸 0~5> [이름필터]"));
			return;
		}

		const int32 EquipSlot = FCString::Atoi(*Args[0]);
		const FString NameFilter = Args.IsValidIndex(1) ? Args[1] : FString();

		ForEachCharacter(World, NameFilter, [EquipSlot](ATDCharacterBase& Character)
		{
			UTDItemUseComponent* ItemUse = FindItemUse(Character);
			if (ItemUse == nullptr)
			{
				return;
			}

			ItemUse->ServerUnequipItem(EquipSlot);
			UE_LOG(LogTDDebug, Log, TEXT("%s — 장착칸 %d 해제 요청 전송"),
				*Character.GetName(), EquipSlot);
		});
	}

	static void UseItem(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.UseItem <인벤슬롯> [이름필터]"));
			return;
		}

		const int32 InventorySlot = FCString::Atoi(*Args[0]);
		const FString NameFilter = Args.IsValidIndex(1) ? Args[1] : FString();

		ForEachCharacter(World, NameFilter, [InventorySlot](ATDCharacterBase& Character)
		{
			UTDItemUseComponent* ItemUse = FindItemUse(Character);
			if (ItemUse == nullptr)
			{
				return;
			}

			ItemUse->ServerUseItem(InventorySlot);
			UE_LOG(LogTDDebug, Log, TEXT("%s — 인벤 %d 사용 요청 전송"),
				*Character.GetName(), InventorySlot);
		});
	}

	static void DumpEquipment(const TArray<FString>& Args, UWorld* World)
	{
		const FString NameFilter = Args.IsValidIndex(0) ? Args[0] : FString();

		ForEachCharacter(World, NameFilter, [](ATDCharacterBase& Character)
		{
			const UTDItemUseComponent* ItemUse = FindItemUse(Character);
			if (ItemUse == nullptr)
			{
				return;
			}

			UE_LOG(LogTDDebug, Log, TEXT("%s — 장착 %d / %d"),
				*Character.GetName(), ItemUse->GetEquippedItems().Num(), UTDItemUseComponent::EquipSlotCount);

			TArray<FTDItemInstance> SortedEquipped = ItemUse->GetEquippedItems();
			SortedEquipped.Sort([](const FTDItemInstance& A, const FTDItemInstance& B)
			{
				return A.SlotIndex < B.SlotIndex;
			});

			const APlayerController* PC = Cast<APlayerController>(Character.GetController());

			for (const FTDItemInstance& Item : SortedEquipped)
			{
				// 강화 배율을 함께 찍는다. 스탯창 숫자가 왜 그 값인지 여기서 바로 대조할 수 있다.
				// 1강당 상승폭은 배율에서 역산한다 — 아이템마다 다르므로(RequiredLevel) 눈으로 봐야 한다.
				const float Multiplier = ItemUse->GetEnhanceMultiplier(Item.ItemId, Item.EnhanceLevel);
				const float PercentPerLevel = Item.EnhanceLevel > 0
					? (Multiplier - 1.f) / Item.EnhanceLevel * 100.f
					: 0.f;

				UE_LOG(LogTDDebug, Log, TEXT("    [%d] %-24s  강화 +%-2d  배율 x%.3f (%.1f%%/강)  옵션 %d개  등급 %s"),
					Item.SlotIndex, *Item.ItemId.ToString(), Item.EnhanceLevel,
					Multiplier, PercentPerLevel,
					Item.Options.Num(), *Item.OptionRarity.ToString());

				// 배율이 실제 스탯으로 얼마가 되는지. 위의 x1.147 만 보고는 아이템마다
				// 기본값이 달라 암산이 되지 않는다.
				for (const FTDTooltipLine& Line :
					UTDTooltipStatics::MakeItemStatLines(PC, Item.ItemId, Item.EnhanceLevel))
				{
					UE_LOG(LogTDDebug, Log, TEXT("          %-20s %s"),
						*Line.Label.ToString(), *Line.Value.ToString());
				}

				// 한 단계 더 올렸을 때. 강화 창이 "성공하면 이렇게 됩니다" 를 띄울 때와
				// 같은 호출이다(EnhanceLevel + 1).
				for (const FTDTooltipLine& Line :
					UTDTooltipStatics::MakeItemStatLines(PC, Item.ItemId, Item.EnhanceLevel + 1))
				{
					UE_LOG(LogTDDebug, Log, TEXT("        → %-20s %s"),
						*Line.Label.ToString(), *Line.Value.ToString());
				}
			}
		});
	}

	/**
	 * 파티 상태를 찍는다. 서버·클라이언트 어느 쪽에서 쳐도 자기가 아는 것을 보여준다 —
	 * PartyId 는 전원에게 복제되므로 클라이언트도 정확한 값을 안다.
	 */
	static void DumpParty(const TArray<FString>& Args, UWorld* World)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (GameState == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.DumpParty: GameState 가 없다."));
			return;
		}

		UE_LOG(LogTDDebug, Log, TEXT("── 접속자 %d명 ──"), GameState->PlayerArray.Num());

		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			const ATDPlayerState* TDPlayerState = Cast<ATDPlayerState>(PlayerState);
			const UTDPartyComponent* Party =
				TDPlayerState ? TDPlayerState->GetPartyComponent() : nullptr;

			if (Party == nullptr)
			{
				continue;
			}

			if (!Party->IsInParty())
			{
				UE_LOG(LogTDDebug, Log, TEXT("  %-20s  파티 없음"),
					*TDPlayerState->GetPlayerName());
				continue;
			}

			UE_LOG(LogTDDebug, Log, TEXT("  %-20s  %s  %d명  경험치 +%.0f%%  파티 %s"),
				*TDPlayerState->GetPlayerName(),
				Party->IsPartyLeader() ? TEXT("[파티장]") : TEXT("        "),
				Party->GetPartyMemberCount(),
				Party->GetExpBonusRate() * 100.f,
				*Party->GetPartyId().ToString(EGuidFormats::DigitsWithHyphens).Left(8));
		}
	}

	/** 이름으로 상대를 찾아 초대한다. UI 가 없어도 파티를 검증할 수 있게 하는 통로다. */
	static void PartyInvite(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("사용법: TD.PartyInvite <상대이름일부>   (TD.DumpParty 로 이름 확인)"));
			return;
		}

		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;
		const ATDPlayerState* SelfState =
			LocalController ? LocalController->GetPlayerState<ATDPlayerState>() : nullptr;

		if (GameState == nullptr || SelfState == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.PartyInvite: 자기 PlayerState 를 찾지 못했다."));
			return;
		}

		ATDPlayerState* Target = nullptr;

		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			ATDPlayerState* TDPlayerState = Cast<ATDPlayerState>(PlayerState);
			if (TDPlayerState == nullptr || TDPlayerState == SelfState)
			{
				continue;
			}

			if (TDPlayerState->GetPlayerName().Contains(Args[0]))
			{
				Target = TDPlayerState;
				break;
			}
		}

		if (Target == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("'%s' 인 접속자를 찾지 못했다."), *Args[0]);
			return;
		}

		// Server RPC 라 클라이언트에서 쳐도 서버까지 간다.
		if (UTDPartyComponent* Party = SelfState->GetPartyComponent())
		{
			Party->ServerInvitePlayer(Target);
			UE_LOG(LogTDDebug, Log, TEXT("%s 로서 %s 에게 파티 초대를 보냈다."),
				*SelfState->GetPlayerName(), *Target->GetPlayerName());
		}
	}

	/** 받은 초대에 응답한다. UI 가 없으므로 명령으로 대신한다. */
	static void PartyAccept(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;
		const ATDPlayerState* SelfState =
			LocalController ? LocalController->GetPlayerState<ATDPlayerState>() : nullptr;

		UTDPartyComponent* Party = SelfState ? SelfState->GetPartyComponent() : nullptr;
		if (Party == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.PartyAccept: PartyComponent 를 찾지 못했다."));
			return;
		}

		const bool bAccept = !Args.IsValidIndex(0) || Args[0].ToBool() || Args[0] == TEXT("1");

		Party->ServerRespondToInvite(bAccept);

		// 어느 플레이어로 응답했는지 함께 찍는다. 2인 PIE 에서는 어느 창에서 쳤는지가
		// 결과를 가르는데, 이름이 없으면 그것을 알 수 없다.
		UE_LOG(LogTDDebug, Log, TEXT("%s 로서 초대에 %s 응답을 보냈다. (결과는 서버 로그에)"),
			*SelfState->GetPlayerName(), bAccept ? TEXT("수락") : TEXT("거절"));
	}

	/**
	 * 처치 경험치 분배를 검증한다. 전투 쪽에 호출부가 붙기 전까지 이걸로 확인한다.
	 *
	 * AwardKillExp 는 서버 권한이 필요하므로 클라이언트 창에서는 동작하지 않는다.
	 * 서버(리슨) 창에서 칠 것.
	 */
	static void PartyExp(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.PartyExp <기본경험치>"));
			return;
		}

		APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;
		const ATDPlayerState* SelfState =
			LocalController ? LocalController->GetPlayerState<ATDPlayerState>() : nullptr;

		const int32 BaseAmount = FCString::Atoi(*Args[0]);

		// 다른 치트와 같은 방식으로 서버까지 태운다. AwardKillExp 가 서버 권한을
		// 요구하므로 클라이언트에서 직접 부르면 조용히 무시된다.
		if (ATDPlayerController* ClientController = GetClientControllerForCheat(World))
		{
			ClientController->ServerDebugPartyExp(BaseAmount);
			UE_LOG(LogTDDebug, Log,
				TEXT("서버에 처치 경험치 %d 분배를 요청했다. (결과는 서버 로그에)"), BaseAmount);
			return;
		}

		UTDPartyComponent* Party = SelfState ? SelfState->GetPartyComponent() : nullptr;
		if (Party == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.PartyExp: PartyComponent 를 찾지 못했다."));
			return;
		}

		const int32 Awarded = Party->AwardKillExp(BaseAmount);

		UE_LOG(LogTDDebug, Log,
			TEXT("처치 경험치 %d 분배 → %d명이 받았다 (보너스 +%.0f%%, 존 '%s' 기준)"),
			BaseAmount, Awarded, Party->GetExpBonusRate() * 100.f,
			*SelfState->GetCurrentZoneId().ToString());
	}

	static void PartyLeave(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;
		const ATDPlayerState* SelfState =
			LocalController ? LocalController->GetPlayerState<ATDPlayerState>() : nullptr;

		if (UTDPartyComponent* Party = SelfState ? SelfState->GetPartyComponent() : nullptr)
		{
			Party->ServerLeaveParty();
			UE_LOG(LogTDDebug, Log, TEXT("파티 탈퇴를 요청했다."));
		}
	}

	/**
	 * 테스트 캐릭터를 넣고 곧바로 하나를 고른다. 테스트할 때마다 두 명령을 치는 수고를 없앤다.
	 *
	 * **RPC 를 두 번 보내지 않고 하나로 합친 이유**가 있다. 지급은 PlayerController 의,
	 * 선택은 PlayerState 의 RPC 라 서로 다른 액터이고, 다른 액터의 Reliable RPC 는
	 * 도착 순서가 보장되지 않는다(§11-G). 뒤바뀌면 "목록이 비었다" 로 실패한다.
	 */
	static void QuickStart(const TArray<FString>& Args, UWorld* World)
	{
		ATDPlayerController* Controller = GetClientControllerForCheat(World);
		if (Controller == nullptr)
		{
			Controller = World != nullptr
				? Cast<ATDPlayerController>(World->GetFirstPlayerController())
				: nullptr;
		}

		if (Controller == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.Start: PlayerController 를 찾지 못했다."));
			return;
		}


		// 기본은 1번(Mage, Lv.12). 스탯이 충분히 올라 있어 전투·회복 테스트에 편하다.
		const int32 SlotIndex = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 1;

		Controller->ServerDebugQuickStart(SlotIndex);

		UE_LOG(LogTDDebug, Log,
			TEXT("테스트 캐릭터 지급 + %d번 선택을 요청했다. (결과는 서버 로그에)"), SlotIndex);
	}

	/** 자기 퀵슬롯 컴포넌트. 서버·클라 어느 쪽에서 쳐도 자기 것을 집는다. */
	/** 로컬 플레이어의 성장 컴포넌트. 스킬 레벨과 잔여 포인트가 여기 있다. */
	static UTDProgressionComponent* GetLocalProgression(UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const ATDPlayerState* PlayerState = PC ? PC->GetPlayerState<ATDPlayerState>() : nullptr;

		return PlayerState ? PlayerState->GetProgressionComponent() : nullptr;
	}

	static void DumpSkills(const TArray<FString>& Args, UWorld* World)
	{
		const UTDProgressionComponent* Progression = GetLocalProgression(World);
		if (Progression == nullptr)
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("TD.DumpSkill: ProgressionComponent 를 찾지 못했다. 캐릭터를 먼저 선택할 것."));
			return;
		}

		const TArray<FName> Skills = Progression->GetClassSkills();

		UE_LOG(LogTDDebug, Log, TEXT("── 직업 '%s' 의 스킬 %d종  (잔여 포인트 %d) ──"),
			*Progression->GetClassId().ToString(), Skills.Num(), Progression->GetRemainingSkillPoints());

		if (Skills.Num() == 0)
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("  비어 있다. DT_Skill 이 지정되지 않았거나 ClassId 가 맞는 행이 없다."));
			return;
		}

		for (const FName& SkillId : Skills)
		{
			FTDSkillRow Row;
			if (!Progression->GetSkillInfo(SkillId, Row))
			{
				continue;
			}

			const int32 SkillLevel = Progression->GetSkillLevel(SkillId);

			// 왜 못 찍는지까지 찍는다. "회색인 이유" 를 눈으로 확인하려는 것이다.
			const TCHAR* Blocked = TEXT("");
			if (!Progression->CanUpgradeSkill(SkillId))
			{
				Blocked = SkillLevel >= Row.MaxLevel
					? TEXT("  ← 최대")
					: (Progression->GetLevel() < Row.RequiredLevel
						? TEXT("  ← 레벨 부족")
						: TEXT("  ← 포인트 부족"));
			}

			UE_LOG(LogTDDebug, Log, TEXT("  %-18s %-8s Lv %d/%d  (요구 레벨 %d)%s"),
				*SkillId.ToString(),
				Row.SkillType == ETDSkillType::Active ? TEXT("[액티브]") : TEXT("[패시브]"),
				SkillLevel, Row.MaxLevel, Row.RequiredLevel, Blocked);

			// 툴팁에 실제로 나갈 문장. 위젯 없이 서식 인자가 제대로 채워지는지 보려는 것이다 —
			// 이름을 잘못 쓰면 {Damage} 가 글자 그대로 남으므로 여기서 바로 드러난다.
			const FText Description =
				UTDTooltipStatics::FormatSkillDescription(Progression, SkillId, SkillLevel);

			if (!Description.IsEmpty())
			{
				UE_LOG(LogTDDebug, Log, TEXT("      %s"), *Description.ToString());
			}

			// 한 단계 올렸을 때의 문장. 스킬을 실제로 찍지 않고도 수치가 따라오는지 확인한다.
			if (SkillLevel < Row.MaxLevel)
			{
				const FText NextDescription =
					UTDTooltipStatics::FormatSkillDescription(Progression, SkillId, SkillLevel + 1);

				if (!NextDescription.IsEmpty())
				{
					UE_LOG(LogTDDebug, Log, TEXT("      → %s"), *NextDescription.ToString());
				}
			}
		}
	}

	static void SkillUp(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("사용법: TD.SkillUp <스킬ID>   예) TD.SkillUp Warrior_Tough   (목록은 TD.DumpSkill)"));
			return;
		}

		UTDProgressionComponent* Progression = GetLocalProgression(World);
		if (Progression == nullptr)
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("TD.SkillUp: ProgressionComponent 를 찾지 못했다. 캐릭터를 먼저 선택할 것."));
			return;
		}

		// Server RPC 라 클라이언트에서 쳐도 서버까지 간다. 거부 사유는 서버 로그에 남는다.
		Progression->ServerUpgradeSkill(FName(*Args[0]));

		UE_LOG(LogTDDebug, Log,
			TEXT("스킬 '%s' 강화를 요청했다. (결과는 TD.DumpSkill, 스탯 반영은 TD.DumpStats)"), *Args[0]);
	}

	/** 로컬 플레이어의 장착·사용 컴포넌트. 재굴림이 여기 있다(장착 아이템도 굴려야 하므로). */
	static UTDItemUseComponent* GetLocalItemUse(UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const ATDPlayerState* PlayerState = PC ? PC->GetPlayerState<ATDPlayerState>() : nullptr;

		return PlayerState ? PlayerState->GetItemUseComponent() : nullptr;
	}

	static void Reroll(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("사용법: TD.Reroll <슬롯> [equip]   세 번째 인자를 주면 장착 칸을 굴린다"));
			return;
		}

		UTDItemUseComponent* ItemUse = GetLocalItemUse(World);
		if (ItemUse == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.Reroll: ItemUseComponent 를 찾지 못했다."));
			return;
		}

		const int32 SlotIndex = FCString::Atoi(*Args[0]);
		const bool bEquipped = Args.IsValidIndex(1);

		UE_LOG(LogTDDebug, Log, TEXT("%s 칸 %d 재굴림을 요청했다. (비용 %d, 결과는 TD.DumpOptions)"),
			bEquipped ? TEXT("장착") : TEXT("인벤"), SlotIndex,
			ItemUse->GetRerollCost(SlotIndex, bEquipped));

		// Server RPC 라 클라이언트에서 쳐도 서버까지 간다.
		ItemUse->ServerRerollOptions(SlotIndex, bEquipped);
	}

	static void DumpOptions(const TArray<FString>& Args, UWorld* World)
	{
		// GetLocalInventory 는 이 아래에 있어 여기서는 직접 꺼낸다.
		const APlayerController* LocalPC = World ? World->GetFirstPlayerController() : nullptr;
		const ATDPlayerState* LocalState = LocalPC ? LocalPC->GetPlayerState<ATDPlayerState>() : nullptr;

		const UTDInventoryComponent* Inventory =
			LocalState ? LocalState->GetInventoryComponent() : nullptr;
		const UTDItemUseComponent* ItemUse = GetLocalItemUse(World);

		if (Inventory == nullptr || ItemUse == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.DumpOptions: 컴포넌트를 찾지 못했다."));
			return;
		}

		// 굴려진 옵션이 어떤 스탯인지는 정의 테이블을 봐야 알 수 있다.
		// 그 테이블의 주인은 ItemUseComponent 라 조회도 그쪽을 거친다.
		auto DumpOne = [LocalPC](const TCHAR* Where, const FTDItemInstance& Item)
		{
			if (Item.Options.Num() == 0 && !Item.OptionRarity.IsValid())
			{
				return;
			}

			UE_LOG(LogTDDebug, Log, TEXT("  [%s %d] %s  등급 %s"),
				Where, Item.SlotIndex, *Item.ItemId.ToString(),
				Item.OptionRarity.IsValid() ? *Item.OptionRarity.ToString() : TEXT("(없음)"));

			for (const FTDItemOption& Option : Item.Options)
			{
				// 굴려진 값과 화면에 나갈 문장을 나란히 둔다. 0.07 이 "7%" 로 보이는지,
				// 소수점이 잘리지 않는지(DecimalPlaces) 를 한 줄에서 대조할 수 있다.
				UE_LOG(LogTDDebug, Log, TEXT("      %-28s %8.4f   %s"),
					*Option.OptionId.ToString(), Option.Value,
					*UTDTooltipStatics::FormatItemOption(LocalPC, Option).ToString());
			}
		};

		UE_LOG(LogTDDebug, Log, TEXT("── 추가 옵션 ──"));

		for (const FTDItemInstance& Item : ItemUse->GetEquippedItems())
		{
			DumpOne(TEXT("장착"), Item);
		}

		for (const FTDItemInstance& Item : Inventory->GetItems())
		{
			DumpOne(TEXT("인벤"), Item);
		}
	}

	static void DumpShop(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("사용법: TD.Shop <상점ID>   예) TD.Shop Shop_Forest"));
			return;
		}

		const FName ShopId(*Args[0]);

		FTDShopRow ShopRow;
		if (!UTDShopStatics::GetShopInfo(ShopId, ShopRow))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("TD.Shop: DT_Shop 에 '%s' 행이 없다. 프로젝트 세팅 > TD > Shop 의 테이블도 확인할 것."),
				*ShopId.ToString());
			return;
		}

		// 인벤토리를 넘기는 것은 되팔기 값 때문이다 — 아이템 정의의 주인이 그쪽이다.
		// 캐릭터를 아직 안 골랐으면 구매가만 찍히고 되팔기가 0 으로 나온다.
		// (GetLocalInventory 는 이 아래에 있어 여기서는 직접 꺼낸다)
		const APlayerController* LocalPC = World ? World->GetFirstPlayerController() : nullptr;
		const ATDPlayerState* LocalState = LocalPC ? LocalPC->GetPlayerState<ATDPlayerState>() : nullptr;
		const UTDInventoryComponent* Inventory =
			LocalState ? LocalState->GetInventoryComponent() : nullptr;

		const TArray<FTDShopEntry> Entries = UTDShopStatics::GetShopEntries(Inventory, ShopId);

		UE_LOG(LogTDDebug, Log, TEXT("── %s (%s) — %d종 ──"),
			*ShopRow.DisplayName.ToString(), *ShopId.ToString(), Entries.Num());

		for (const FTDShopEntry& Entry : Entries)
		{
			UE_LOG(LogTDDebug, Log, TEXT("  %-16s 구매 %6d   되팔기 %6d"),
				*Entry.ItemId.ToString(), Entry.Price, Entry.SellBackPrice);
		}
	}

	static void ShopBuy(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(1))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("사용법: TD.ShopBuy <상점ID> <아이템ID> [개수=1]"));
			return;
		}

		ATDPlayerController* Controller = World != nullptr
			? Cast<ATDPlayerController>(World->GetFirstPlayerController())
			: nullptr;

		if (Controller == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.ShopBuy: PlayerController 를 찾지 못했다."));
			return;
		}

		const int32 Count = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : 1;

		// Server RPC 라 클라이언트에서 쳐도 서버까지 간다. 사거리 검증도 서버가 한다.
		Controller->ServerBuyFromShop(FName(*Args[0]), FName(*Args[1]), Count);

		UE_LOG(LogTDDebug, Log, TEXT("'%s' %d개 구매를 요청했다. (결과는 상점 결과 로그)"),
			*Args[1], Count);
	}

	static void ShopSell(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(1))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("사용법: TD.ShopSell <상점ID> <인벤슬롯> [개수=1]   (칸 번호는 TD.DumpInventory)"));
			return;
		}

		ATDPlayerController* Controller = World != nullptr
			? Cast<ATDPlayerController>(World->GetFirstPlayerController())
			: nullptr;

		if (Controller == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.ShopSell: PlayerController 를 찾지 못했다."));
			return;
		}

		const int32 SlotIndex = FCString::Atoi(*Args[1]);
		const int32 Count = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : 1;

		Controller->ServerSellToShop(FName(*Args[0]), SlotIndex, Count);

		UE_LOG(LogTDDebug, Log, TEXT("%d번 칸 %d개 판매를 요청했다. (결과는 상점 결과 로그)"),
			SlotIndex, Count);
	}

	static void LearnSkills(const TArray<FString>& Args, UWorld* World)
	{
		// 인자가 없으면 1레벨. TD.LearnSkills 0 은 전부 초기화다.
		const int32 SkillLevel = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 1;

		ATDPlayerController* Controller = GetClientControllerForCheat(World);
		if (Controller == nullptr)
		{
			Controller = World != nullptr
				? Cast<ATDPlayerController>(World->GetFirstPlayerController())
				: nullptr;
		}

		if (Controller == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.LearnSkills: PlayerController 를 찾지 못했다."));
			return;
		}

		// Server RPC 라 클라이언트에서 쳐도 서버까지 간다.
		Controller->ServerDebugLearnSkills(SkillLevel);

		UE_LOG(LogTDDebug, Log,
			TEXT("스킬 전부를 레벨 %d 로 맞추도록 요청했다. (결과는 TD.DumpSkill)"), SkillLevel);
	}

	static void UseSkill(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.UseSkill <자리1~3>   Q·W·E 를 누른 것과 같다"));
			return;
		}

		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		ATDPlayerCharacter* Character = PC ? Cast<ATDPlayerCharacter>(PC->GetPawn()) : nullptr;
		UTDSkillComponent* Skills = Character ? Character->GetSkillComponent() : nullptr;

		if (Skills == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.UseSkill: SkillComponent 를 찾지 못했다."));
			return;
		}

		const int32 SlotIndex = FCString::Atoi(*Args[0]);

		// Server RPC 라 클라이언트에서 쳐도 서버까지 간다. 거부 사유는 서버 로그에 남는다.
		Skills->ServerUseSkillSlot(SlotIndex);

		UE_LOG(LogTDDebug, Log, TEXT("스킬 자리 %d 사용을 요청했다."), SlotIndex);
	}

	static void DumpCooldowns(const TArray<FString>& Args, UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const ATDPlayerCharacter* Character = PC ? Cast<ATDPlayerCharacter>(PC->GetPawn()) : nullptr;
		const UTDSkillComponent* Skills = Character ? Character->GetSkillComponent() : nullptr;
		const UTDProgressionComponent* Progression = GetLocalProgression(World);

		if (Skills == nullptr || Progression == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.DumpCooldown: 스킬·성장 컴포넌트를 찾지 못했다."));
			return;
		}

		UE_LOG(LogTDDebug, Log, TEXT("── 시전 상태 ──"));
		UE_LOG(LogTDDebug, Log, TEXT("  시전 중: %s"),
			Skills->IsCasting() ? *Skills->GetCastingSkillId().ToString() : TEXT("(없음)"));

		for (int32 Slot = 1; Slot <= 3; ++Slot)
		{
			const FName SkillId = Progression->GetSkillForSlot(Slot);
			if (SkillId.IsNone())
			{
				UE_LOG(LogTDDebug, Log, TEXT("  %d.  (비어 있음)"), Slot);
				continue;
			}

			const float Remaining = Skills->GetCooldownRemaining(SkillId);
			UE_LOG(LogTDDebug, Log, TEXT("  %d.  %-18s Lv %d   쿨 %.1f초"),
				Slot, *SkillId.ToString(), Progression->GetSkillLevel(SkillId), Remaining);
		}
	}

	static UTDQuickSlotComponent* GetLocalQuickSlots(UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const ATDPlayerState* PlayerState = PC ? PC->GetPlayerState<ATDPlayerState>() : nullptr;

		return PlayerState ? PlayerState->GetQuickSlotComponent() : nullptr;
	}

	static void DumpQuickSlots(const TArray<FString>& Args, UWorld* World)
	{
		const UTDQuickSlotComponent* QuickSlots = GetLocalQuickSlots(World);
		if (QuickSlots == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.DumpQuick: QuickSlotComponent 를 찾지 못했다."));
			return;
		}

		UE_LOG(LogTDDebug, Log, TEXT("── 퀵슬롯 %d칸 ──"), UTDQuickSlotComponent::SlotCount);

		for (int32 i = 0; i < UTDQuickSlotComponent::SlotCount; ++i)
		{
			const FTDQuickSlot Slot = QuickSlots->GetSlot(i);

			if (Slot.IsEmpty())
			{
				UE_LOG(LogTDDebug, Log, TEXT("  %d.  (비어 있음)"), i);
				continue;
			}

			// 아이템이면 인벤토리에 몇 개 있는지 함께 찍는다. 0 이면 회색으로 표시될 자리다.
			const int32 Count = QuickSlots->GetSlotItemCount(i);

			UE_LOG(LogTDDebug, Log, TEXT("  %d.  %-16s 보유 %d개%s"),
				i,
				*Slot.Id.ToString(),
				Count,
				Count == 0 ? TEXT(" ← 사용 불가") : TEXT(""));
		}
	}

	/** 로컬 플레이어의 인벤토리. 서버·클라 어느 쪽에서 쳐도 자기 것을 집는다. */
	static UTDInventoryComponent* GetLocalInventory(UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const ATDPlayerState* PlayerState = PC ? PC->GetPlayerState<ATDPlayerState>() : nullptr;

		return PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	}

	/** 인벤토리 슬롯 하나를 강화한다. UI 가 붙기 전까지 확률표를 검증하는 통로다. */
	static void EnhanceItem(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.Enhance <인벤슬롯>"));
			return;
		}

		UTDInventoryComponent* Inventory = GetLocalInventory(World);
		if (Inventory == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.Enhance: InventoryComponent 를 찾지 못했다."));
			return;
		}

		// Server RPC 라 클라이언트에서 쳐도 서버까지 간다. 결과는 ClientItemEnhanced 로 돌아온다.
		Inventory->ServerEnhanceItem(FCString::Atoi(*Args[0]));

		UE_LOG(LogTDDebug, Log, TEXT("슬롯 %s 강화를 요청했다. (결과는 서버 로그 + 응답으로)"), *Args[0]);
	}

	/** 여러 번 굴려 성공/하락/실패 비율을 확인한다. 확률표가 의도대로 읽히는지 감으로 볼 때 쓴다. */
	/**
	 * 확률표대로 굴러가는지 본다. **아이템도 골드도 쓰지 않는다.**
	 *
	 * 실제 슬롯을 반복 강화하면 10강 근처에서 골드가 먼저 떨어져 표본이 모이지 않는다.
	 * 판정 자체는 TDEnhance::Roll 이라는 순수 함수라, 주사위만 따로 굴리면 같은 분포가 나온다.
	 * 서버 권한도 필요 없다 — DT_Enhance 는 클라이언트도 갖고 있다.
	 */
	static void EnhanceStress(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("사용법: TD.EnhanceStress <목표강화단수> [횟수=1000]   예) TD.EnhanceStress 10 5000"));
			return;
		}

		const UTDInventoryComponent* Inventory = GetLocalInventory(World);
		if (Inventory == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.EnhanceStress: InventoryComponent 를 찾지 못했다."));
			return;
		}

		const int32 Level = FCString::Atoi(*Args[0]);

		FTDEnhanceRow Row;
		if (!Inventory->GetEnhanceInfo(Level, Row))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("TD.EnhanceStress: %d강 행이 없다. DT_Enhance 미지정이거나 최고 단수를 넘었다."), Level);
			return;
		}

		const int32 Iterations = Args.IsValidIndex(1) ? FMath::Max(1, FCString::Atoi(*Args[1])) : 1000;

		int32 SuccessCount = 0;
		int32 DowngradeCount = 0;
		int32 NoChangeCount = 0;
		int32 TotalTiersLost = 0;

		for (int32 i = 0; i < Iterations; ++i)
		{
			const FTDEnhanceRollResult Result =
				TDEnhance::Roll(Row, FMath::FRand(), FMath::FRand(), FMath::FRand());

			switch (Result.Outcome)
			{
			case ETDEnhanceOutcome::Success:
				++SuccessCount;
				break;

			case ETDEnhanceOutcome::Downgraded:
				++DowngradeCount;
				TotalTiersLost += Result.DowngradeTiers;
				break;

			default:
				++NoChangeCount;
				break;
			}
		}

		const float ToPercent = 100.f / Iterations;

		UE_LOG(LogTDDebug, Log, TEXT("%d강 시도 %d회 — 표 기준: 성공 %.0f%%, 실패 시 하락 %.0f%% (%d~%d단계), 비용 %d"),
			Level, Iterations, Row.SuccessRate * 100.f, Row.DowngradeChanceOnFail * 100.f,
			Row.MinDowngradeTiers, Row.MaxDowngradeTiers, Row.Cost);

		UE_LOG(LogTDDebug, Log, TEXT("    성공     %6d  (%.1f%%)"), SuccessCount, SuccessCount * ToPercent);
		UE_LOG(LogTDDebug, Log, TEXT("    하락     %6d  (%.1f%%)  평균 %.2f단계"),
			DowngradeCount, DowngradeCount * ToPercent,
			DowngradeCount > 0 ? static_cast<float>(TotalTiersLost) / DowngradeCount : 0.f);
		UE_LOG(LogTDDebug, Log, TEXT("    변화없음 %6d  (%.1f%%)"), NoChangeCount, NoChangeCount * ToPercent);
	}

	/**
	 * 착용 레벨제한에 따라 강화 배율이 어떻게 달라지는지 표로 찍는다.
	 *
	 * 실제 아이템이 없어도 확인할 수 있다. RequiredLevel 이 다른 장비 두 개를
	 * 같은 단수까지 올려 비교하는 것은 골드가 너무 많이 든다.
	 */
	static void EnhanceCurve(const TArray<FString>& Args, UWorld* World)
	{
		// 인자를 주면 그 착용레벨 하나만, 안 주면 양 끝과 중간을 함께 보여준다.
		TArray<int32> RequiredLevels;
		if (Args.IsValidIndex(0))
		{
			RequiredLevels.Add(FCString::Atoi(*Args[0]));
		}
		else
		{
			RequiredLevels = { 0, 10, 25, 40, 50 };
		}

		UE_LOG(LogTDDebug, Log, TEXT("강화 배율 — 착용레벨이 높은 장비일수록 1강당 많이 오른다"));

		for (const int32 RequiredLevel : RequiredLevels)
		{
			const float PercentPerLevel = TDEnhance::GetStatPercentPerLevel(RequiredLevel);

			UE_LOG(LogTDDebug, Log, TEXT("  착용Lv %-3d  %.1f%%/강    5강 x%.3f   10강 x%.3f   18강 x%.3f"),
				RequiredLevel, PercentPerLevel * 100.f,
				TDEnhance::GetStatMultiplier(5, RequiredLevel),
				TDEnhance::GetStatMultiplier(10, RequiredLevel),
				TDEnhance::GetStatMultiplier(18, RequiredLevel));
		}
	}

	// ── UI ────────────────────────────────────────────────

	/**
	 * UI 루트(WBP_Root)를 띄운다. 창은 전부 그 안에 붙으므로 이것이 없으면
	 * 어떤 창도 열리지 않는다.
	 *
	 * **임시 통로다.** 지금 루트를 띄우는 코드는 ATDUITestPlayerController 에만 있어서,
	 * 정식 게임모드로 실행하면 UI 가 하나도 나오지 않는다. 실제로 누가 띄울지
	 * (PlayerController / HUD / 서브시스템)는 UI 담당이 정할 몫이라, 그때까지
	 * 다른 시스템을 정상 데이터로 테스트할 수 있게 해 두는 것이다.
	 */
	static void ShowUIRoot(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		if (Controller == nullptr || !Controller->IsLocalController())
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("TD.ShowUIRoot: 로컬 PlayerController 가 없다."));
			return;
		}

		// ATDUITestPlayerController 가 쓰는 것과 같은 경로다. 그쪽이 바뀌면 여기도 바꿔야 한다.
		const TCHAR* RootPath = TEXT("/Game/UI/WBP_Root.WBP_Root_C");

		UClass* RootClass = LoadClass<UUserWidget>(nullptr, RootPath);
		if (RootClass == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.ShowUIRoot: '%s' 를 찾지 못했다."), RootPath);
			return;
		}

		UUserWidget* Root = CreateWidget<UUserWidget>(Controller, RootClass);
		if (Root == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.ShowUIRoot: 위젯 생성에 실패했다."));
			return;
		}

		// 뷰포트에 올라가는 순간 NativeConstruct 가 스스로 UI 관리자에 등록한다.
		Root->AddToViewport();
		Controller->bShowMouseCursor = true;

		UE_LOG(LogTDDebug, Log, TEXT("UI 루트를 띄웠다. 이제 TD.OpenSettings 로 창을 열 수 있다."));
	}

	/**
	 * 설정 창을 연다(토글). 레벨 블루프린트로 띄우는 것과 달리 **로컬 플레이어에서**
	 * 부르므로, 서버 인스턴스에서 실행되어 null 이 나오는 문제가 생기지 않는다.
	 *
	 * 단계마다 로그를 남긴다. 창이 안 뜨는 원인이 매번 다른 자리에 있어서,
	 * 어디까지 갔는지 보이지 않으면 짐작으로 뒤지게 된다.
	 */
	static void OpenSettings(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		if (Controller == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.OpenSettings: PlayerController 가 없다."));
			return;
		}

		ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
		if (LocalPlayer == nullptr)
		{
			// 서버가 들고 있는 원격 컨트롤러다. UI 는 화면이 있는 쪽에만 있다.
			UE_LOG(LogTDDebug, Warning,
				TEXT("TD.OpenSettings: 로컬 플레이어가 없다. 클라이언트 창에서 칠 것."));
			return;
		}

		// 창 클래스가 지정되지 않으면 서브시스템이 조용히 돌아간다. 먼저 확인해 준다.
		const UTDUISettings* UISettings = GetDefault<UTDUISettings>();
		if (UISettings == nullptr || UISettings->SystemWindowClass.IsNull())
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("TD.OpenSettings: System Window Class 가 비어 있다. "
					 "프로젝트 세팅 > Game > TD UI > Windows 에서 WBP_SettingsWindow 를 지정할 것."));
			return;
		}

		UTDUIManagerSubsystem* UI = LocalPlayer->GetSubsystem<UTDUIManagerSubsystem>();
		if (UI == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.OpenSettings: UIManagerSubsystem 을 찾지 못했다."));
			return;
		}

		UE_LOG(LogTDDebug, Log, TEXT("설정 창 토글을 요청했다. (창 클래스 '%s')"),
			*UISettings->SystemWindowClass.ToString());

		// 실제로 열리지 않으면 여기서부터는 서브시스템이 이유를 로그로 남긴다
		// (WBP_Root 미등록 등).
		UI->RequestMenu(ETDNavMenuType::System);
	}

	// ── 채팅 ──────────────────────────────────────────────

	/**
	 * 콘솔 인자를 다시 한 문장으로 합친다.
	 *
	 * 콘솔이 공백마다 인자를 잘라 주기 때문에, 문장을 받으려면 되돌려야 한다.
	 */
	static FString JoinArgs(const TArray<FString>& Args, int32 StartIndex)
	{
		FString Result;

		for (int32 Index = StartIndex; Index < Args.Num(); ++Index)
		{
			if (!Result.IsEmpty())
			{
				Result.AppendChar(TEXT(' '));
			}
			Result += Args[Index];
		}

		return Result;
	}

	/**
	 * 채팅을 보낸다. ServerSendChat 이 Server RPC 라 클라이언트 창에서도 그대로 통한다.
	 *
	 * 정상 경로를 그대로 타므로 검열·길이·쿨다운 검사도 전부 거친다 —
	 * 치트로 우회하면 그 검사들을 확인할 수 없다.
	 */
	static void SendChat(UWorld* World, ETDChatChannel Channel,
		const FString& Message, const FString& TargetName)
	{
		APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		ATDPlayerController* TDController = Cast<ATDPlayerController>(Controller);

		if (TDController == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("채팅: PlayerController 를 찾지 못했다."));
			return;
		}

		TDController->ServerSendChat(Channel, Message, TargetName);
	}

	static void Say(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.Say <할 말>"));
			return;
		}

		SendChat(World, ETDChatChannel::All, JoinArgs(Args, 0), FString());
	}

	static void SayParty(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.SayParty <할 말>"));
			return;
		}

		SendChat(World, ETDChatChannel::Party, JoinArgs(Args, 0), FString());
	}

	static void Whisper(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(1))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("사용법: TD.Whisper <상대이름> <할 말>   (TD.DumpParty 로 이름 확인)"));
			return;
		}

		SendChat(World, ETDChatChannel::Whisper, JoinArgs(Args, 1), Args[0]);
	}

	/**
	 * 필터만 돌려 본다. **채팅을 보내지 않는다.**
	 *
	 * 금지어를 추가한 뒤 오탐이 없는지 확인하는 용도다. 정상 문장을 넣어 보고
	 * 가려지지 않는지 보는 쪽이 실제로 더 중요하다 — 못 잡는 것보다 멀쩡한 말이
	 * 가려지는 쪽이 불만이 크다.
	 */
	static void ChatFilterTest(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.ChatFilter <검사할 문장>"));
			return;
		}

		const UTDChatSettings* Settings = UTDChatSettings::Get();
		UDataTable* Table = Settings ? Settings->BannedWordTable.LoadSynchronous() : nullptr;

		if (Table == nullptr)
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("TD.ChatFilter: 금지어 테이블이 없다. 프로젝트 세팅 > TD > Chat 을 확인할 것."));
			return;
		}

		TArray<FString> BannedWords;
		Table->ForeachRow<FTDBannedWordRow>(TEXT("ChatFilterTest"),
			[&BannedWords](const FName&, const FTDBannedWordRow& Row)
			{
				if (!Row.Word.IsEmpty())
				{
					BannedWords.Add(Row.Word);
				}
			});

		const FString Input = JoinArgs(Args, 0);

		bool bMasked = false;
		const FString Output = TDChatFilter::Mask(Input, BannedWords, bMasked);

		TArray<int32> SourceIndex;
		const FString Normalized = TDChatFilter::Normalize(Input, SourceIndex);

		UE_LOG(LogTDDebug, Log, TEXT("금지어 %d개로 검사"), BannedWords.Num());
		UE_LOG(LogTDDebug, Log, TEXT("    입력   %s"), *Input);
		UE_LOG(LogTDDebug, Log, TEXT("    정규화 %s"), *Normalized);
		UE_LOG(LogTDDebug, Log, TEXT("    결과   %s  %s"),
			*Output, bMasked ? TEXT("← 걸림") : TEXT("(통과)"));
	}

	// ── 거래소 ────────────────────────────────────────────
	// 전부 Server RPC 를 타므로 클라이언트 창에서도 그대로 통한다.

	static ATDPlayerController* GetLocalTDController(UWorld* World)
	{
		return World ? Cast<ATDPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	static void MarketList(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(1))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("사용법: TD.MarketList <인벤슬롯> <가격>   가격은 묶음 전체 값이다"));
			return;
		}

		if (ATDPlayerController* Controller = GetLocalTDController(World))
		{
			Controller->ServerListItem(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		}
	}

	static void MarketBuy(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("사용법: TD.MarketBuy <매물번호>   (TD.MarketSearch 로 번호 확인)"));
			return;
		}

		if (ATDPlayerController* Controller = GetLocalTDController(World))
		{
			Controller->ServerBuyListing(FCString::Atoi(*Args[0]));
		}
	}

	static void MarketCancel(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.MarketCancel <매물번호>"));
			return;
		}

		if (ATDPlayerController* Controller = GetLocalTDController(World))
		{
			Controller->ServerCancelListing(FCString::Atoi(*Args[0]));
		}
	}

	/**
	 * 매물을 찾는다. 결과는 서버가 Client RPC 로 돌려주므로 로그에 한 박자 늦게 찍힌다.
	 *
	 * 인자를 주지 않으면 전체, 주면 그 아이템만 본다.
	 */
	static void MarketSearch(const TArray<FString>& Args, UWorld* World)
	{
		const FName Filter = Args.IsValidIndex(0) ? FName(*Args[0]) : NAME_None;
		const int32 Page = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 0;

		if (ATDPlayerController* Controller = GetLocalTDController(World))
		{
			Controller->ServerSearchListings(Filter, Page);
		}
	}

	static void MarketMine(const TArray<FString>& Args, UWorld* World)
	{
		if (ATDPlayerController* Controller = GetLocalTDController(World))
		{
			Controller->ServerRequestMyListings();
		}
	}

	/**
	 * 거래소 상태를 서버 쪽에서 직접 찍는다. 검색과 달리 RPC 를 타지 않아
	 * 매물이 실제로 어떻게 들어 있는지 그대로 보인다.
	 */
	static void DumpMarket(const TArray<FString>& Args, UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const UTDMarketSubsystem* Market = GameInstance
			? GameInstance->GetSubsystem<UTDMarketSubsystem>()
			: nullptr;

		if (Market == nullptr || World == nullptr || World->GetNetMode() == NM_Client)
		{
			// 매물은 서버 메모리에만 있다. 클라이언트에는 애초에 읽을 것이 없다.
			UE_LOG(LogTDDebug, Warning,
				TEXT("TD.DumpMarket 은 서버(또는 단일 PIE)에서만 쓴다 — 매물은 서버에만 있다. "
					 "클라이언트 창에서는 TD.MarketSearch 를 쓸 것(결과가 한 박자 늦게 찍힌다)."));
			return;
		}

		// 페이지를 넉넉히 잡아 한 번에 본다. 검증용이라 페이징이 의미가 없다.
		const TArray<FTDMarketListing> All = Market->Search(NAME_None, 0);

		UE_LOG(LogTDDebug, Log, TEXT("거래소 매물 %d건 (첫 페이지)"), All.Num());

		for (const FTDMarketListing& Entry : All)
		{
			UE_LOG(LogTDDebug, Log, TEXT("    [%d] %-20s x%-3d  %8d 골드   판매자 %s  강화 +%d"),
				Entry.ListingId, *Entry.Item.ItemId.ToString(), Entry.Item.Count,
				Entry.Price, *Entry.SellerName, Entry.Item.EnhanceLevel);
		}
	}

	static void QuickSet(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(1))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("사용법: TD.QuickSet <슬롯0~%d> <아이템ID>   예) TD.QuickSet 0 Elixir"),
				UTDQuickSlotComponent::SlotCount - 1);
			return;
		}

		UTDQuickSlotComponent* QuickSlots = GetLocalQuickSlots(World);
		if (QuickSlots == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.QuickSet: QuickSlotComponent 를 찾지 못했다."));
			return;
		}

		const int32 SlotIndex = FCString::Atoi(*Args[0]);
		const FName Id(*Args[1]);

		// Server RPC 라 클라이언트에서 쳐도 서버까지 간다.
		QuickSlots->ServerSetSlot(SlotIndex, ETDQuickSlotType::Item, Id);

		UE_LOG(LogTDDebug, Log, TEXT("퀵슬롯 %d 에 '%s' 등록을 요청했다. (결과는 TD.DumpQuick)"),
			SlotIndex, *Id.ToString());
	}

	static void QuickUse(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.QuickUse <슬롯>"));
			return;
		}

		UTDQuickSlotComponent* QuickSlots = GetLocalQuickSlots(World);
		if (QuickSlots == nullptr)
		{
			return;
		}

		// 입력 바인딩과 **같은 경로**를 탄다. 여기서 통과하면 키를 눌렀을 때도 동작한다.
		QuickSlots->ServerUseSlot(FCString::Atoi(*Args[0]));

		UE_LOG(LogTDDebug, Log, TEXT("퀵슬롯 %s 사용을 요청했다."), *Args[0]);
	}

	static void QuickClear(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.QuickClear <슬롯>"));
			return;
		}

		if (UTDQuickSlotComponent* QuickSlots = GetLocalQuickSlots(World))
		{
			QuickSlots->ServerClearSlot(FCString::Atoi(*Args[0]));
			UE_LOG(LogTDDebug, Log, TEXT("퀵슬롯 %s 비우기를 요청했다."), *Args[0]);
		}
	}

	/** 리매핑 목록을 찍는다. IA 설정이 제대로 됐는지 확인하는 용도다. */
	static void DumpKeys(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (PC == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.DumpKeys: PlayerController 를 찾지 못했다."));
			return;
		}

		const TArray<FTDKeyMappingRow> Rows = UTDInputSettingsLibrary::GetKeyMappings(PC);

		// 비어 있으면 라이브러리가 이미 원인을 로그로 남긴다.
		UE_LOG(LogTDDebug, Log, TEXT("── 리매핑 가능한 키 %d개 ──"), Rows.Num());

		for (const FTDKeyMappingRow& Row : Rows)
		{
			UE_LOG(LogTDDebug, Log, TEXT("  %-14s %-12s %s"),
				*Row.MappingName.ToString(),
				*Row.CurrentKey.ToString(),
				*Row.DisplayName.ToString());
		}
	}

	/** 부활 버튼을 대신한다. UI 가 붙기 전까지 사망·부활을 검증하는 통로다. */
	static void Respawn(const TArray<FString>& Args, UWorld* World)
	{
		ATDPlayerController* Controller = GetClientControllerForCheat(World);
		if (Controller == nullptr)
		{
			Controller = World != nullptr
				? Cast<ATDPlayerController>(World->GetFirstPlayerController())
				: nullptr;
		}

		if (Controller == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.Respawn: PlayerController 를 찾지 못했다."));
			return;
		}

		// 치트가 아니라 정식 경로를 그대로 쓴다. 버튼이 부를 함수와 같아야
		// 여기서 통과한 것이 실제로도 동작한다.
		Controller->ServerRequestRespawn();
		UE_LOG(LogTDDebug, Log, TEXT("서버에 부활을 요청했다. (결과는 서버 로그에)"));
	}

	static void TravelToZone(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("사용법: TD.Zone <존태그> [진입점]   예) TD.Zone Zone.Region1.Field02 West"));
			return;
		}

		const FName EntryName = Args.IsValidIndex(1) ? FName(*Args[1]) : NAME_None;

		// 태그가 등록돼 있지 않으면 여기서 걸러낸다. 서버까지 보내면 "테이블에 없다"는
		// 엉뚱한 사유가 찍혀 오타인지 데이터 누락인지 구분되지 않는다.
		const FGameplayTag ZoneTag =
			FGameplayTag::RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound=*/ false);

		if (!ZoneTag.IsValid())
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("'%s' 는 등록된 게임플레이 태그가 아니다. 오타이거나 태그가 없다."), *Args[0]);
			return;
		}

		ATDPlayerController* Controller = GetClientControllerForCheat(World);
		if (Controller == nullptr)
		{
			// 서버(또는 단일 PIE)에서는 로컬 컨트롤러를 직접 쓴다.
			Controller = World != nullptr
				? Cast<ATDPlayerController>(World->GetFirstPlayerController())
				: nullptr;
		}

		if (Controller == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.Zone: PlayerController 를 찾지 못했다."));
			return;
		}

		Controller->ServerDebugTravelToZone(ZoneTag, EntryName);
		UE_LOG(LogTDDebug, Log,
			TEXT("서버에 '%s'%s 로 이동을 요청했다. (레벨 제한은 우회하지 않는다 / 결과는 서버 로그에)"),
			*ZoneTag.ToString(),
			EntryName.IsNone() ? TEXT("") : *FString::Printf(TEXT(" (진입점 %s)"), *EntryName.ToString()));
	}

	static void AddExp(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.AddExp <경험치> [이름필터]"));
			return;
		}

		const int32 Amount = FCString::Atoi(*Args[0]);
		const FString NameFilter = Args.IsValidIndex(1) ? Args[1] : FString();

		if (ATDPlayerController* ClientController = GetClientControllerForCheat(World))
		{
			ClientController->ServerDebugAddExp(Amount);
			UE_LOG(LogTDDebug, Log,
				TEXT("서버에 경험치 %d 지급을 요청했다. (자기 캐릭터만 / 결과는 서버 로그에)"), Amount);
			return;
		}

		ForEachCharacter(World, NameFilter, [Amount](ATDCharacterBase& Character)
		{
			UTDProgressionComponent* Progression = Character.GetProgressionComponent();
			if (Progression == nullptr)
			{
				return;
			}

			Progression->AddExp(Amount);

			UE_LOG(LogTDDebug, Log, TEXT("%s — 경험치 +%d → Level %d, 누적 %d, 다음까지 %d (%.0f%%)"),
				*Character.GetName(), Amount,
				Progression->GetLevel(),
				Progression->GetExp(),
				Progression->GetExpToNextLevel(),
				Progression->GetLevelProgress() * 100.f);
		});
	}

	static void SetClass(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.SetClass <ClassId> [이름필터]"));
			return;
		}

		const FName NewClassId(*Args[0]);
		const FString NameFilter = Args.IsValidIndex(1) ? Args[1] : FString();

		if (ATDPlayerController* ClientController = GetClientControllerForCheat(World))
		{
			ClientController->ServerDebugSetClass(NewClassId);
			UE_LOG(LogTDDebug, Log, TEXT("서버에 직업 '%s' 설정을 요청했다. (자기 캐릭터만)"),
				*NewClassId.ToString());
			return;
		}

		ForEachCharacter(World, NameFilter, [NewClassId](ATDCharacterBase& Character)
		{
			UTDProgressionComponent* Progression = Character.GetProgressionComponent();
			if (Progression == nullptr)
			{
				return;
			}

			const bool bChanged = Progression->SetClassId(NewClassId);
			UE_LOG(LogTDDebug, Log, TEXT("%s — 직업 '%s' 설정 %s"),
				*Character.GetName(), *NewClassId.ToString(), bChanged ? TEXT("성공") : TEXT("실패"));
		});
	}

	// ── 캐릭터 선택 (D51~D58) ─────────────────────────────
	// 선택 전에는 Pawn 이 없으므로 ForEachCharacter 로는 찾을 수 없다.
	// PlayerController 를 거쳐 PlayerState 를 직접 잡는다.

	static void ForEachPlayerState(UWorld* World, TFunctionRef<void(ATDPlayerState&)> Func)
	{
		if (World == nullptr)
		{
			return;
		}

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			if (Controller == nullptr)
			{
				continue;
			}

			if (ATDPlayerState* PlayerState = Controller->GetPlayerState<ATDPlayerState>())
			{
				Func(*PlayerState);
			}
		}
	}

	static void GiveGold(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.GiveGold <금액>"));
			return;
		}

		const int32 Amount = FCString::Atoi(*Args[0]);

		if (ATDPlayerController* ClientController = GetClientControllerForCheat(World))
		{
			ClientController->ServerDebugGiveGold(Amount);
			UE_LOG(LogTDDebug, Log,
				TEXT("서버에 골드 %d 지급을 요청했다. (결과는 서버 로그에)"), Amount);
			return;
		}

		ForEachPlayerState(World, [Amount](ATDPlayerState& PlayerState)
		{
			if (UTDInventoryComponent* Inventory = PlayerState.GetInventoryComponent())
			{
				const bool bAdded = Inventory->AddGold(Amount);
				UE_LOG(LogTDDebug, Log, TEXT("%s — 골드 %d 지급 %s (보유 %d)"),
					*PlayerState.GetPlayerName(), Amount,
					bAdded ? TEXT("성공") : TEXT("실패"), Inventory->GetGold());
			}
		});
	}

	static void DumpCharacters(const TArray<FString>& Args, UWorld* World)
	{
		int32 Found = 0;

		ForEachPlayerState(World, [&Found](ATDPlayerState& PlayerState)
		{
			++Found;

			const TArray<FTDCharacterSummary>& Slots = PlayerState.GetCharacterSlots();

			UE_LOG(LogTDDebug, Log, TEXT("%s — 보유 %d개, 선택 %s"),
				*PlayerState.GetPlayerName(),
				Slots.Num(),
				PlayerState.HasSelectedCharacter() ? TEXT("완료") : TEXT("아직"));

			if (Slots.IsEmpty())
			{
				UE_LOG(LogTDDebug, Warning,
					TEXT("    목록이 비어 있다. 세이브가 아직 없으므로 TD.GiveTestCharacters 로 채울 것."));
				return;
			}

			for (int32 Index = 0; Index < Slots.Num(); ++Index)
			{
				const FTDCharacterSummary& Slot = Slots[Index];
				UE_LOG(LogTDDebug, Log, TEXT("    [%d] %-12s %-10s Lv.%d  (장착 %d개)"),
					Index,
					*Slot.CharacterName,
					*Slot.ClassId.ToString(),
					Slot.Level,
					Slot.EquippedItemIds.Num());
			}
		});

		if (Found == 0)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("PlayerState 를 찾지 못했다."));
		}
	}

	static void GiveTestCharacters(const TArray<FString>& Args, UWorld* World)
	{
		// 목록을 채우는 것은 서버 권한이다. Play As Client 로 띄우면 서버 콘솔이 없으므로
		// 다른 치트와 마찬가지로 컨트롤러의 Server RPC 를 태운다.
		if (ATDPlayerController* ClientController = GetClientControllerForCheat(World))
		{
			ClientController->ServerDebugGiveTestCharacters();
			UE_LOG(LogTDDebug, Log,
				TEXT("서버에 테스트 캐릭터 지급을 요청했다. (자기 계정만. 결과는 TD.DumpCharacters 로 확인)"));
			return;
		}

		ForEachPlayerState(World, [](ATDPlayerState& PlayerState)
		{
			TArray<FTDCharacterSummary> Slots;

			FTDCharacterSummary& First = Slots.AddDefaulted_GetRef();
			First.CharacterName = TEXT("테스트A");
			First.ClassId = FName(TEXT("Warrior"));
			First.Level = 25;

			FTDCharacterSummary& Second = Slots.AddDefaulted_GetRef();
			Second.CharacterName = TEXT("테스트B");
			Second.ClassId = FName(TEXT("Mage"));
			Second.Level = 12;

			FTDCharacterSummary& Third = Slots.AddDefaulted_GetRef();
			Third.CharacterName = TEXT("테스트C");
			Third.ClassId = FName(TEXT("Archer"));
			Third.Level = 1;

			PlayerState.SetCharacterSlots(MoveTemp(Slots));

			UE_LOG(LogTDDebug, Log, TEXT("%s — 테스트 캐릭터 3개를 넣었다."), *PlayerState.GetPlayerName());
		});
	}

	static void SelectCharacter(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.SelectCharacter <슬롯번호>"));
			return;
		}

		const int32 SlotIndex = FCString::Atoi(*Args[0]);

		// 이쪽은 클라이언트에서도 된다. ServerSelectCharacter 가 PlayerState 의 RPC 라
		// 소유 체인을 타고 서버에 도착한다 — UI 가 쓸 경로와 같다.
		if (World != nullptr && World->GetNetMode() == NM_Client)
		{
			APlayerController* Controller = World->GetFirstPlayerController();
			ATDPlayerState* PlayerState = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;

			if (PlayerState == nullptr)
			{
				UE_LOG(LogTDDebug, Warning, TEXT("PlayerState 를 찾지 못했다."));
				return;
			}

			PlayerState->ServerSelectCharacter(SlotIndex);
			UE_LOG(LogTDDebug, Log,
				TEXT("서버에 %d번 캐릭터 선택을 요청했다. (결과는 TD.DumpCharacters 로 확인)"), SlotIndex);
			return;
		}

		ForEachPlayerState(World, [SlotIndex](ATDPlayerState& PlayerState)
		{
			const bool bSelected = PlayerState.SelectCharacter(SlotIndex);
			UE_LOG(LogTDDebug, Log, TEXT("%s — %d번 캐릭터 선택 %s"),
				*PlayerState.GetPlayerName(), SlotIndex, bSelected ? TEXT("성공") : TEXT("실패"));
		});
	}
	
	static void Damage(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(0))
		{
			UE_LOG(LogTDDebug, Warning, TEXT("사용법: TD.Damage <양> [이름필터]"));
			return;
		}

		const float Amount = FCString::Atof(*Args[0]);
		const FString NameFilter = Args.IsValidIndex(1) ? Args[1] : FString();

		// ApplyRawDamage 는 서버 권한을 요구한다. 클라이언트 창에서 그냥 부르면
		// 조용히 무시되고 로그만 "피해 적용됨"처럼 보여 오해하기 쉽다.
		if (ATDPlayerController* ClientController = GetClientControllerForCheat(World))
		{
			ClientController->ServerDebugDamage(Amount);
			UE_LOG(LogTDDebug, Log,
				TEXT("서버에 %.0f 피해를 요청했다. (자기 캐릭터만 / 결과는 서버 로그에)"), Amount);
			return;
		}

		ForEachCharacter(World, NameFilter, [Amount](ATDCharacterBase& Character)
		{
			UTDCombatStatics::ApplyRawDamage(&Character, Amount);

			// 결과 확인. 어트리뷰트는 ASC 를 통해 읽는다.
			if (UAbilitySystemComponent* ASC = Character.GetAbilitySystemComponent())
			{
				UE_LOG(LogTDDebug, Log, TEXT("%s — %.0f 피해. 체력 %.1f / %.1f%s"),
					*Character.GetName(), Amount,
					ASC->GetNumericAttribute(UTDAttributeSet::GetHealthAttribute()),
					ASC->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute()),
					Character.IsDead() ? TEXT(" [사망]") : TEXT(""));
			}
		});
	}

	static void Hit(const TArray<FString>& Args, UWorld* World)
	{
		const FString NameFilter = Args.IsValidIndex(0) ? Args[0] : FString();

		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		ATDCharacterBase* Attacker = PC ? Cast<ATDCharacterBase>(PC->GetPawn()) : nullptr;
		if (Attacker == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.Hit: 플레이어 캐릭터가 없다. TD.SelectCharacter 로 먼저 스폰할 것."));
			return;
		}

		ForEachCharacter(World, NameFilter, [Attacker](ATDCharacterBase& Target)
		{
			if (&Target == Attacker)
			{
				return;   // 자해 방지
			}

			const FTDDamageResult Result = UTDCombatStatics::ApplyDamage(
				Attacker, &Target, FGameplayTagContainer());

			UE_LOG(LogTDDebug, Log, TEXT("%s → %s — %.1f 피해%s%s"),
				*Attacker->GetName(), *Target.GetName(), Result.FinalDamage,
				Result.bCritical ? TEXT(" (크리티컬!)") : TEXT(""),
				Target.IsDead() ? TEXT(" [사망]") : TEXT(""));
		});
	}
	
	static void Attack(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		ATDCharacterBase* Attacker = PC ? Cast<ATDCharacterBase>(PC->GetPawn()) : nullptr;

		if (Attacker == nullptr || Attacker->GetCombatComponent() == nullptr)
		{
			UE_LOG(LogTDDebug, Warning, TEXT("TD.Attack: 플레이어 캐릭터가 없다."));
			return;
		}

		Attacker->GetCombatComponent()->ServerRequestAttack();
	}
}

static FAutoConsoleCommandWithWorldAndArgs GTDDumpStats(
	TEXT("TD.DumpStats"),
	TEXT("캐릭터의 모든 스탯을 출력한다. 사용법: TD.DumpStats [이름필터]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::DumpStats));

static FAutoConsoleCommandWithWorldAndArgs GTDSetLevel(
	TEXT("TD.SetLevel"),
	TEXT("레벨을 설정하고 성장 스탯을 갱신한다. 사용법: TD.SetLevel <레벨> [이름필터]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::SetLevel));

static FAutoConsoleCommandWithWorldAndArgs GTDAddExp(
	TEXT("TD.AddExp"),
	TEXT("경험치를 지급하고 레벨업을 판정한다. 사용법: TD.AddExp <경험치> [이름필터]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::AddExp));

static FAutoConsoleCommandWithWorldAndArgs GTDDumpSkill(
	TEXT("TD.DumpSkill"),
	TEXT("내 직업의 스킬 목록과 찍은 레벨을 찍는다. 사용법: TD.DumpSkill"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::DumpSkills));

static FAutoConsoleCommandWithWorldAndArgs GTDSkillUp(
	TEXT("TD.SkillUp"),
	TEXT("스킬을 한 단계 올린다. 사용법: TD.SkillUp <스킬ID>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::SkillUp));

static FAutoConsoleCommandWithWorldAndArgs GTDReroll(
	TEXT("TD.Reroll"),
	TEXT("추가 옵션을 다시 굴린다. 사용법: TD.Reroll <슬롯> [equip]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::Reroll));

static FAutoConsoleCommandWithWorldAndArgs GTDDumpOptions(
	TEXT("TD.DumpOptions"),
	TEXT("장착·인벤토리 아이템의 추가 옵션을 찍는다. 사용법: TD.DumpOptions"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::DumpOptions));

static FAutoConsoleCommandWithWorldAndArgs GTDShop(
	TEXT("TD.Shop"),
	TEXT("상점이 파는 목록과 되팔기 값을 찍는다. 사용법: TD.Shop <상점ID>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::DumpShop));

static FAutoConsoleCommandWithWorldAndArgs GTDShopBuy(
	TEXT("TD.ShopBuy"),
	TEXT("상점에서 산다(사거리 검증 있음). 사용법: TD.ShopBuy <상점ID> <아이템ID> [개수=1]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::ShopBuy));

static FAutoConsoleCommandWithWorldAndArgs GTDShopSell(
	TEXT("TD.ShopSell"),
	TEXT("상점에 판다. 사용법: TD.ShopSell <상점ID> <인벤슬롯> [개수=1]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::ShopSell));

static FAutoConsoleCommandWithWorldAndArgs GTDLearnSkills(
	TEXT("TD.LearnSkills"),
	TEXT("내 직업의 스킬을 전부 지정 레벨로 맞춘다(레벨 조건·포인트 무시). 사용법: TD.LearnSkills [레벨=1]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::LearnSkills));

static FAutoConsoleCommandWithWorldAndArgs GTDUseSkill(
	TEXT("TD.UseSkill"),
	TEXT("Q·W·E 자리의 스킬을 쓴다. 사용법: TD.UseSkill <자리1~3>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::UseSkill));

static FAutoConsoleCommandWithWorldAndArgs GTDDumpCooldown(
	TEXT("TD.DumpCooldown"),
	TEXT("시전 상태와 스킬 쿨타임을 찍는다. 사용법: TD.DumpCooldown"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::DumpCooldowns));

static FAutoConsoleCommandWithWorldAndArgs GTDDumpParty(
	TEXT("TD.DumpParty"),
	TEXT("접속자들의 파티 상태를 찍는다. 사용법: TD.DumpParty"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::DumpParty));

static FAutoConsoleCommandWithWorldAndArgs GTDPartyInvite(
	TEXT("TD.PartyInvite"),
	TEXT("이름으로 찾아 파티에 초대한다. 사용법: TD.PartyInvite <상대이름일부>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::PartyInvite));

static FAutoConsoleCommandWithWorldAndArgs GTDPartyAccept(
	TEXT("TD.PartyAccept"),
	TEXT("받은 초대에 응답한다. 사용법: TD.PartyAccept [0=거절]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::PartyAccept));

static FAutoConsoleCommandWithWorldAndArgs GTDPartyExp(
	TEXT("TD.PartyExp"),
	TEXT("처치 경험치를 파티에 분배한다(서버 전용). 사용법: TD.PartyExp <기본경험치>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::PartyExp));

static FAutoConsoleCommandWithWorldAndArgs GTDPartyLeave(
	TEXT("TD.PartyLeave"),
	TEXT("파티에서 나간다. 사용법: TD.PartyLeave"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::PartyLeave));

static FAutoConsoleCommandWithWorldAndArgs GTDStart(
	TEXT("TD.Start"),
	TEXT("테스트 캐릭터를 넣고 곧바로 선택한다. 사용법: TD.Start [슬롯=1]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::QuickStart));

static FAutoConsoleCommandWithWorldAndArgs GTDEnhance(
	TEXT("TD.Enhance"),
	TEXT("인벤토리 아이템을 한 단계 강화한다. 사용법: TD.Enhance <인벤슬롯>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::EnhanceItem));

static FAutoConsoleCommandWithWorldAndArgs GTDEnhanceStress(
	TEXT("TD.EnhanceStress"),
	TEXT("확률 판정만 여러 번 굴려 분포를 본다(아이템·골드 안 씀). 사용법: TD.EnhanceStress <목표강화단수> [횟수=1000]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::EnhanceStress));

static FAutoConsoleCommandWithWorldAndArgs GTDEnhanceCurve(
	TEXT("TD.EnhanceCurve"),
	TEXT("착용레벨별 강화 배율을 표로 찍는다. 사용법: TD.EnhanceCurve [착용레벨]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::EnhanceCurve));

static FAutoConsoleCommandWithWorldAndArgs GTDShowUIRoot(
	TEXT("TD.ShowUIRoot"),
	TEXT("UI 루트(WBP_Root)를 띄운다. 창을 열려면 먼저 이것이 있어야 한다. 사용법: TD.ShowUIRoot"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::ShowUIRoot));

static FAutoConsoleCommandWithWorldAndArgs GTDOpenSettings(
	TEXT("TD.OpenSettings"),
	TEXT("설정 창을 연다(토글). 사용법: TD.OpenSettings"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::OpenSettings));

static FAutoConsoleCommandWithWorldAndArgs GTDSay(
	TEXT("TD.Say"),
	TEXT("전체 채팅을 보낸다. 사용법: TD.Say <할 말>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::Say));

static FAutoConsoleCommandWithWorldAndArgs GTDSayParty(
	TEXT("TD.SayParty"),
	TEXT("파티 채팅을 보낸다. 사용법: TD.SayParty <할 말>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::SayParty));

static FAutoConsoleCommandWithWorldAndArgs GTDWhisper(
	TEXT("TD.Whisper"),
	TEXT("귓속말을 보낸다. 사용법: TD.Whisper <상대이름> <할 말>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::Whisper));

static FAutoConsoleCommandWithWorldAndArgs GTDChatFilter(
	TEXT("TD.ChatFilter"),
	TEXT("금지어 필터만 돌려 본다(채팅을 보내지 않는다). 사용법: TD.ChatFilter <문장>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::ChatFilterTest));

static FAutoConsoleCommandWithWorldAndArgs GTDMarketList(
	TEXT("TD.MarketList"),
	TEXT("아이템을 거래소에 올린다. 사용법: TD.MarketList <인벤슬롯> <가격>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::MarketList));

static FAutoConsoleCommandWithWorldAndArgs GTDMarketBuy(
	TEXT("TD.MarketBuy"),
	TEXT("매물을 산다. 사용법: TD.MarketBuy <매물번호>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::MarketBuy));

static FAutoConsoleCommandWithWorldAndArgs GTDMarketCancel(
	TEXT("TD.MarketCancel"),
	TEXT("자기 매물을 내린다. 사용법: TD.MarketCancel <매물번호>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::MarketCancel));

static FAutoConsoleCommandWithWorldAndArgs GTDMarketSearch(
	TEXT("TD.MarketSearch"),
	TEXT("매물을 찾는다(결과는 한 박자 늦게 찍힌다). 사용법: TD.MarketSearch [ItemId] [페이지]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::MarketSearch));

static FAutoConsoleCommandWithWorldAndArgs GTDMarketMine(
	TEXT("TD.MarketMine"),
	TEXT("내가 올린 매물을 본다. 사용법: TD.MarketMine"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::MarketMine));

static FAutoConsoleCommandWithWorldAndArgs GTDDumpMarket(
	TEXT("TD.DumpMarket"),
	TEXT("거래소 매물을 서버에서 직접 찍는다(서버 전용). 사용법: TD.DumpMarket"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::DumpMarket));

static FAutoConsoleCommandWithWorldAndArgs GTDDumpQuick(
	TEXT("TD.DumpQuick"),
	TEXT("퀵슬롯 6칸과 각 칸의 보유 개수를 찍는다. 사용법: TD.DumpQuick"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::DumpQuickSlots));

static FAutoConsoleCommandWithWorldAndArgs GTDQuickSet(
	TEXT("TD.QuickSet"),
	TEXT("퀵슬롯에 등록한다. 사용법: TD.QuickSet <슬롯> <아이템ID> [skill]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::QuickSet));

static FAutoConsoleCommandWithWorldAndArgs GTDQuickUse(
	TEXT("TD.QuickUse"),
	TEXT("퀵슬롯을 사용한다(키 입력과 같은 경로). 사용법: TD.QuickUse <슬롯>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::QuickUse));

static FAutoConsoleCommandWithWorldAndArgs GTDQuickClear(
	TEXT("TD.QuickClear"),
	TEXT("퀵슬롯을 비운다. 사용법: TD.QuickClear <슬롯>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::QuickClear));

static FAutoConsoleCommandWithWorldAndArgs GTDDumpKeys(
	TEXT("TD.DumpKeys"),
	TEXT("리매핑 가능한 키 목록을 찍는다. 사용법: TD.DumpKeys"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::DumpKeys));

static FAutoConsoleCommandWithWorldAndArgs GTDRespawn(
	TEXT("TD.Respawn"),
	TEXT("죽었으면 되살아난다(부활 버튼과 같은 경로). 사용법: TD.Respawn"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::Respawn));

static FAutoConsoleCommandWithWorldAndArgs GTDZone(
	TEXT("TD.Zone"),
	TEXT("지정한 존으로 이동한다(레벨 제한은 그대로 적용). 사용법: TD.Zone <존태그>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::TravelToZone));

static FAutoConsoleCommandWithWorldAndArgs GTDSetClass(
	TEXT("TD.SetClass"),
	TEXT("직업을 설정하고 성장 스탯을 갱신한다. 사용법: TD.SetClass <ClassId> [이름필터]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::SetClass));

static FAutoConsoleCommandWithWorldAndArgs GTDGiveGold(
	TEXT("TD.GiveGold"),
	TEXT("골드를 지급한다. 사용법: TD.GiveGold <금액>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::GiveGold));

static FAutoConsoleCommandWithWorldAndArgs GTDGiveItem(
	TEXT("TD.GiveItem"),
	TEXT("아이템을 지급한다. 사용법: TD.GiveItem <ItemId> [개수] [이름필터]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::GiveItem));

static FAutoConsoleCommandWithWorldAndArgs GTDGiveItemTest(
	TEXT("TD.GiveItemTest"),
	TEXT("테스트용 소비 아이템 4종을 한 번에 지급한다. 사용법: TD.GiveItemTest [개수=20]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::GiveItemTest));

static FAutoConsoleCommandWithWorldAndArgs GTDDumpInventory(
	TEXT("TD.DumpInventory"),
	TEXT("인벤토리 내용을 슬롯별로 출력한다. 사용법: TD.DumpInventory [이름필터]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::DumpInventory));

static FAutoConsoleCommandWithWorldAndArgs GTDEquip(
	TEXT("TD.Equip"),
	TEXT("인벤토리의 장신구를 장착한다. 사용법: TD.Equip <인벤슬롯> <장착칸 0~5> [이름필터]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::EquipItem));

static FAutoConsoleCommandWithWorldAndArgs GTDUnequip(
	TEXT("TD.Unequip"),
	TEXT("장착을 해제한다. 사용법: TD.Unequip <장착칸 0~5> [이름필터]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::UnequipItem));

static FAutoConsoleCommandWithWorldAndArgs GTDUseItem(
	TEXT("TD.UseItem"),
	TEXT("소비 아이템을 사용한다. 사용법: TD.UseItem <인벤슬롯> [이름필터]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::UseItem));

static FAutoConsoleCommandWithWorldAndArgs GTDDumpEquipment(
	TEXT("TD.DumpEquipment"),
	TEXT("장착 상태를 출력한다. 사용법: TD.DumpEquipment [이름필터]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::DumpEquipment));

static FAutoConsoleCommandWithWorldAndArgs GTDDumpCharacters(
	TEXT("TD.DumpCharacters"),
	TEXT("캐릭터 선택 목록과 선택 여부를 출력한다. 사용법: TD.DumpCharacters"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::DumpCharacters));

static FAutoConsoleCommandWithWorldAndArgs GTDGiveTestCharacters(
	TEXT("TD.GiveTestCharacters"),
	TEXT("테스트용 캐릭터 3개를 목록에 넣는다(서버 전용). 세이브가 붙으면 필요 없어진다."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::GiveTestCharacters));

static FAutoConsoleCommandWithWorldAndArgs GTDSelectCharacter(
	TEXT("TD.SelectCharacter"),
	TEXT("캐릭터를 선택하고 Pawn 을 스폰한다. 사용법: TD.SelectCharacter <슬롯번호>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::SelectCharacter));

static FAutoConsoleCommandWithWorldAndArgs GTDDamage(
	TEXT("TD.Damage"),
	TEXT("고정 수치의 피해를 직접 적용한다(공식 미경유). 사용법: TD.Damage <양> [이름필터]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::Damage));

static FAutoConsoleCommandWithWorldAndArgs GTDHit(
	TEXT("TD.Hit"),
	TEXT("플레이어가 대상을 공격한다(스탯·공식 경유). 사용법: TD.Hit [이름필터]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::Hit));

static FAutoConsoleCommandWithWorldAndArgs GTDAttack(
	TEXT("TD.Attack"),
	TEXT("전방 히트박스로 공격한다(쿨타임·팀 판정 포함). 사용법: TD.Attack"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::Attack));

/**
 * 이동 입력의 축 값을 로그로 찍는다. 방향이 이상할 때 IMC 모디파이어를 확인하는 용도다.
 *
 * ATDPlayerCharacter::Move 가 이 값을 읽는다. 매 프레임 찍히므로 필요할 때만 켤 것.
 */
static FAutoConsoleVariableRef GTDInputDebug(
	TEXT("TD.InputDebug"),
	GTDInputDebugValue,
	TEXT("1 이면 이동 입력의 축 값을 로그로 찍는다. 사용법: TD.InputDebug 1"));

#endif // !UE_BUILD_SHIPPING
