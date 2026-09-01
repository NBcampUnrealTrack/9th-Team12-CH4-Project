#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Character/TDCharacterBase.h"
#include "Core/TDGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Items/TDInventoryComponent.h"
#include "Items/TDItemUseComponent.h"
#include "Player/TDPlayerController.h"
#include "Player/TDPlayerState.h"
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

			UE_LOG(LogTDDebug, Log, TEXT("%s  (%d / %d 칸 사용)"),
				*Character.GetName(), Inventory->GetUsedSlotCount(), Inventory->GetSlotCapacity());

			// FastArray 는 배열 순서를 보장하지 않는다. 실제 화면은 SlotIndex 로 그리므로
			// 읽는 사람이 헷갈리지 않도록 출력도 슬롯 순으로 맞춘다.
			TArray<FTDItemInstance> SortedItems = Inventory->GetItems();
			SortedItems.Sort([](const FTDItemInstance& A, const FTDItemInstance& B)
			{
				return A.SlotIndex < B.SlotIndex;
			});

			for (const FTDItemInstance& Item : SortedItems)
			{
				UE_LOG(LogTDDebug, Log, TEXT("    [%3d] %-24s x%-4d  강화 +%d  옵션 %d개"),
					Item.SlotIndex, *Item.ItemId.ToString(), Item.Count,
					Item.EnhanceLevel, Item.Options.Num());
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

			for (const FTDItemInstance& Item : SortedEquipped)
			{
				UE_LOG(LogTDDebug, Log, TEXT("    [%d] %-24s  강화 +%d  옵션 %d개  등급 %s"),
					Item.SlotIndex, *Item.ItemId.ToString(), Item.EnhanceLevel,
					Item.Options.Num(), *Item.OptionRarity.ToString());
			}
		});
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

static FAutoConsoleCommandWithWorldAndArgs GTDZone(
	TEXT("TD.Zone"),
	TEXT("지정한 존으로 이동한다(레벨 제한은 그대로 적용). 사용법: TD.Zone <존태그>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::TravelToZone));

static FAutoConsoleCommandWithWorldAndArgs GTDSetClass(
	TEXT("TD.SetClass"),
	TEXT("직업을 설정하고 성장 스탯을 갱신한다. 사용법: TD.SetClass <ClassId> [이름필터]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::SetClass));

static FAutoConsoleCommandWithWorldAndArgs GTDGiveItem(
	TEXT("TD.GiveItem"),
	TEXT("아이템을 지급한다. 사용법: TD.GiveItem <ItemId> [개수] [이름필터]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TDDebugCommands::GiveItem));

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

#endif // !UE_BUILD_SHIPPING
