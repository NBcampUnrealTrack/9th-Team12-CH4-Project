#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Character/TDCharacterBase.h"
#include "Core/TDGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Items/TDEnhanceStatics.h"
#include "Items/TDInventoryComponent.h"
#include "Items/TDQuickSlotComponent.h"
#include "Party/TDPartyComponent.h"
#include "Settings/TDInputSettingsLibrary.h"
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

			UE_LOG(LogTDDebug, Log, TEXT("  %d.  %-8s %-16s %s"),
				i,
				Slot.Type == ETDQuickSlotType::Item ? TEXT("[아이템]") : TEXT("[스킬]"),
				*Slot.Id.ToString(),
				Slot.Type == ETDQuickSlotType::Item
					? *FString::Printf(TEXT("보유 %d개%s"), Count, Count == 0 ? TEXT(" ← 사용 불가") : TEXT(""))
					: TEXT(""));
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

	static void QuickSet(const TArray<FString>& Args, UWorld* World)
	{
		if (!Args.IsValidIndex(1))
		{
			UE_LOG(LogTDDebug, Warning,
				TEXT("사용법: TD.QuickSet <슬롯0~%d> <아이템ID> [skill]   예) TD.QuickSet 0 Elixir"),
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

		// 세 번째 인자에 아무거나 넣으면 스킬로 등록한다. 스킬 시스템이 없어도
		// 슬롯이 타입을 구분해 저장하는지는 확인할 수 있다.
		const ETDQuickSlotType Type = Args.IsValidIndex(2)
			? ETDQuickSlotType::Skill
			: ETDQuickSlotType::Item;

		// Server RPC 라 클라이언트에서 쳐도 서버까지 간다.
		QuickSlots->ServerSetSlot(SlotIndex, Type, Id);

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
