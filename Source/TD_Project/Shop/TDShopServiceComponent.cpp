#include "Shop/TDShopServiceComponent.h"

#include "Character/TDPlayerCharacter.h"
#include "Data/TDItemRow.h"
#include "Data/TDShopRow.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TDDialogueSource.h"
#include "Interaction/TDInteractionFlowComponent.h"
#include "Items/TDInventoryComponent.h"
#include "Player/TDPlayerController.h"
#include "Player/TDPlayerState.h"
#include "Settings/TDShopSettings.h"
#include "Shop/TDShopComponent.h"
#include "Shop/TDShopStatics.h"
#include "UI/Core/TDUIManagerSubsystem.h"
#include "UI/Layer/WindowLayer/Shop/TDShopWindowWidget.h"
#include "World/TDNPCBase.h"

namespace
{
	FString ShopFailureMessage(ETDShopResult Result)
	{
		switch (Result)
		{
		case ETDShopResult::ShopNotFound:
			return TEXT("상점 데이터를 찾을 수 없습니다.");
		case ETDShopResult::TooFar:
			return TEXT("상인에게서 너무 멀어졌습니다.");
		case ETDShopResult::ItemNotSold:
			return TEXT("이 상점에서 판매하지 않는 아이템입니다.");
		case ETDShopResult::NotEnoughGold:
			return TEXT("골드가 부족합니다.");
		case ETDShopResult::InventoryFull:
			return TEXT("인벤토리 공간이 부족합니다.");
		case ETDShopResult::ItemNotFound:
			return TEXT("해당 아이템이 가방에 없습니다.");
		case ETDShopResult::ItemNotTradable:
			return TEXT("퀘스트 아이템은 판매할 수 없습니다.");
		case ETDShopResult::InvalidCount:
			return TEXT("구매 수량이 올바르지 않습니다.");
		case ETDShopResult::NoCharacterSelected:
			return TEXT("사용할 캐릭터를 먼저 선택하세요.");
		default:
			return TEXT("거래를 처리하지 못했습니다.");
		}
	}
}

UTDShopServiceComponent::UTDShopServiceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

ATDPlayerController* UTDShopServiceComponent::GetController() const
{
	return Cast<ATDPlayerController>(GetOwner());
}

UTDInventoryComponent* UTDShopServiceComponent::GetInventory() const
{
	const ATDPlayerController* Controller = GetController();
	const ATDPlayerState* State = Controller
		? Controller->GetPlayerState<ATDPlayerState>()
		: nullptr;

	return State ? State->GetInventoryComponent() : nullptr;
}

void UTDShopServiceComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ServerTimer);
		GetWorld()->GetTimerManager().ClearTimer(LocalOpenTimer);
	}

	ActiveSessionId = 0;
	ActiveNPC.Reset();
	ActiveShop.Reset();
	ActivePawn.Reset();
	ActiveInventory.Reset();

	CloseLocalWindow(false);

	Super::EndPlay(EndPlayReason);
}

void UTDShopServiceComponent::StartForNPC(ATDNPCBase* NPC)
{
	ATDPlayerController* Controller = GetController();
	UTDShopComponent* Shop = NPC ? NPC->GetShopComponent() : nullptr;

	if (!Controller
		|| !Controller->HasAuthority()
		|| !IsValid(NPC)
		|| !NPC->IsShopNPC()
		|| !IsValid(Shop)
		|| Shop->GetShopId() != NPC->GetShopId()
		|| !GetWorld())
	{
		return;
	}

	EndService();

	ATDPlayerState* State =
		Controller->GetPlayerState<ATDPlayerState>();

	ActiveNPC = NPC;
	ActiveShop = Shop;
	ActivePawn = Controller->GetPawn();
	ActiveInventory = GetInventory();
	ActiveShopId = NPC->GetShopId();
	StartZone = State ? State->GetCurrentZoneId() : FGameplayTag();

	SessionCounter =
		SessionCounter >= MAX_int32 ? 1 : SessionCounter + 1;

	ActiveSessionId = SessionCounter;
	LastRequestId = 0;
	LastSentRevision = -1;
	LastSentGold = -1;
	bClientOpened = false;
	bProcessingRequest = false;

	if (!IsServerContextValid())
	{
		EndService();
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		ServerTimer,
		this,
		&ThisClass::TickServer,
		0.1f,
		true);
}

bool UTDShopServiceComponent::IsServerContextValid() const
{
	ATDPlayerController* Controller = GetController();
	ATDNPCBase* NPC = ActiveNPC.Get();
	UTDShopComponent* Shop = ActiveShop.Get();
	ATDPlayerCharacter* Character =
		Cast<ATDPlayerCharacter>(ActivePawn.Get());

	if (!Controller
		|| !Controller->HasAuthority()
		|| ActiveSessionId <= 0
		|| ActiveShopId.IsNone()
		|| !IsValid(NPC)
		|| !IsValid(Shop)
		|| Shop != NPC->GetShopComponent()
		|| Shop->GetShopId() != ActiveShopId
		|| NPC->GetShopId() != ActiveShopId
		|| !IsValid(Character)
		|| Controller->GetPawn() != Character
		|| Character->IsDead()
		|| NPC->GetWorld() != Character->GetWorld()
		|| !ActiveInventory.IsValid()
		|| ActiveInventory.Get() != GetInventory())
	{
		return false;
	}

	const ATDPlayerState* State =
		Controller->GetPlayerState<ATDPlayerState>();

	if (!State || State->GetCurrentZoneId() != StartZone)
	{
		return false;
	}

	if (!ITDDialogueSource::Execute_IsDialogueSourceInRange(NPC, Character))
	{
		return false;
	}

	return NPC->CanInteract_Implementation(Character);
}

bool UTDShopServiceComponent::IsActiveForShop(FName ShopId) const
{
	return !ShopId.IsNone()
		&& ShopId == ActiveShopId
		&& IsServerContextValid();
}

void UTDShopServiceComponent::TickServer()
{
	if (!IsServerContextValid())
	{
		EndService();
		return;
	}

	ATDPlayerController* Controller = GetController();
	const UTDInteractionFlowComponent* Flow =
		Controller->FindComponentByClass<UTDInteractionFlowComponent>();

	if (Flow && Flow->IsDialogueActive())
	{
		EndService();
		return;
	}

	if (Flow && Flow->IsChapterPresentationActive())
	{
		if (bClientOpened)
		{
			EndService();
		}
		return;
	}

	UTDInventoryComponent* Inventory = ActiveInventory.Get();

	if (!bClientOpened)
	{
		bClientOpened = true;
		ClientOpenService(
			MakeView(
				TEXT("구매하거나 판매할 아이템을 선택하세요."),
				0,
				true,
				true));
		return;
	}

	if (Inventory->GetInventoryRevision() != LastSentRevision
		|| Inventory->GetGold() != LastSentGold)
	{
		SendView();
	}
}

FTDShopWindowView UTDShopServiceComponent::MakeView(
	const FString& Message,
	int32 ReplyRequestId,
	bool bClearCart,
	bool bClearSellList)
{
	FTDShopWindowView View;
	View.SessionId = ActiveSessionId;
	View.ShopId = ActiveShopId;
	View.Message = Message;
	View.ReplyRequestId = ReplyRequestId;
	View.bClearCart = bClearCart;
	View.bClearSellList = bClearSellList;

	UTDInventoryComponent* Inventory = ActiveInventory.Get();
	if (!Inventory)
	{
		return View;
	}

	FTDShopRow ShopRow;
	if (UTDShopStatics::GetShopInfo(ActiveShopId, ShopRow))
	{
		View.ShopName = ShopRow.DisplayName;
	}

	if (View.ShopName.IsEmpty())
	{
		View.ShopName = FText::FromName(ActiveShopId);
	}

	View.Gold = Inventory->GetGold();
	View.InventoryRevision = Inventory->GetInventoryRevision();
	for (const FTDShopEntry& Source :
		UTDShopStatics::GetShopEntries(Inventory, ActiveShopId))
	{
		const FTDItemRow* Definition =
			Inventory->FindItemDefinition(Source.ItemId);

		if (Definition)
		{
			FTDShopBuyItemView& Entry = View.BuyItems.AddDefaulted_GetRef();
			Entry.ItemId = Source.ItemId;
			Entry.DisplayName = Definition->DisplayName.IsEmpty()
				? FText::FromName(Source.ItemId)
				: Definition->DisplayName;
			Entry.Icon = Definition->Icon;
			Entry.Rarity = Definition->Rarity;
			Entry.Price = Source.Price;
			Entry.bStackable = Definition->bStackable;
		}
	}

	for (const FTDItemInstance& Item : Inventory->GetItems())
	{
		const FTDItemRow* Definition =
			Inventory->FindItemDefinition(Item.ItemId);

		if (!Definition)
		{
			continue;
		}

		FTDShopSellItemView& Entry = View.SellItems.AddDefaulted_GetRef();
		Entry.SlotIndex = Item.SlotIndex;
		Entry.ItemId = Item.ItemId;
		Entry.DisplayName = Definition->DisplayName.IsEmpty()
			? FText::FromName(Item.ItemId)
			: Definition->DisplayName;
		Entry.Icon = Definition->Icon;
		Entry.Rarity = Definition->Rarity;
		Entry.Count = Item.Count;
		Entry.UnitSellPrice = UTDShopStatics::GetSellBackPrice(
			Inventory,
			ActiveShopId,
			Item.ItemId);
		Entry.bCanSell = Definition->bCanDiscard;
		Entry.bStackable = Definition->bStackable;
	}

	View.SellItems.Sort(
		[](const FTDShopSellItemView& A, const FTDShopSellItemView& B)
		{
			return A.SlotIndex < B.SlotIndex;
		});

	LastSentRevision = View.InventoryRevision;
	LastSentGold = View.Gold;

	return View;
}

void UTDShopServiceComponent::SendView(
	const FString& Message,
	int32 ReplyRequestId,
	bool bClearCart,
	bool bClearSellList)
{
	if (ActiveSessionId > 0 && bClientOpened)
	{
		ClientUpdateService(MakeView(
			Message,
			ReplyRequestId,
			bClearCart,
			bClearSellList));
	}
}

bool UTDShopServiceComponent::CanProcessRequest(
	int32 SessionId,
	int64 ExpectedRevision,
	int32 RequestId)
{
	if (SessionId <= 0 || SessionId != ActiveSessionId)
	{
		return false;
	}

	if (!IsServerContextValid())
	{
		EndService();
		return false;
	}

	const UTDInteractionFlowComponent* Flow =
		GetController()->FindComponentByClass<UTDInteractionFlowComponent>();

	if (!bClientOpened
		|| (Flow && (Flow->IsDialogueActive()
			|| Flow->IsChapterPresentationActive())))
	{
		EndService();
		return false;
	}

	if (RequestId <= 0 || RequestId <= LastRequestId || bProcessingRequest)
	{
		return false;
	}

	LastRequestId = RequestId;

	if (ExpectedRevision != ActiveInventory->GetInventoryRevision())
	{
		SendView(TEXT("가방이 변경되었습니다. 다시 시도하세요."), RequestId, false);
		return false;
	}

	return true;
}

void UTDShopServiceComponent::ServerBuyCart_Implementation(
	int32 SessionId,
	int64 ExpectedRevision,
	const TArray<FTDShopCartLine>& Lines,
	int32 RequestId)
{
	if (!CanProcessRequest(SessionId, ExpectedRevision, RequestId))
	{
		return;
	}

	bProcessingRequest = true;

	ETDShopResult Result = ETDShopResult::Success;
	TMap<FName, int32> Counts;
	int32 TotalCount = 0;
	int32 TotalPrice = 0;

	if (Lines.IsEmpty() || Lines.Num() > 128)
	{
		Result = ETDShopResult::InvalidCount;
	}

	TMap<FName, int32> Prices;
	if (Result == ETDShopResult::Success)
	{
		for (const FTDShopEntry& Entry :
			UTDShopStatics::GetShopEntries(ActiveInventory.Get(), ActiveShopId))
		{
			Prices.Add(Entry.ItemId, Entry.Price);
		}
	}

	const UTDShopSettings* Settings = UTDShopSettings::Get();
	const int32 MaxCount = Settings ? Settings->MaxCountPerTrade : 1;

	int64 TotalCount64 = 0;
	int64 TotalPrice64 = 0;

	if (Result == ETDShopResult::Success)
	{
		for (const FTDShopCartLine& Line : Lines)
		{
			const int32* UnitPrice = Prices.Find(Line.ItemId);

			if (Line.ItemId.IsNone() || Line.Count <= 0 || UnitPrice == nullptr)
			{
				Result = UnitPrice ? ETDShopResult::InvalidCount : ETDShopResult::ItemNotSold;
				break;
			}

			int32& CombinedCount = Counts.FindOrAdd(Line.ItemId);
			const int64 NewCombined =
				static_cast<int64>(CombinedCount) + Line.Count;

			if (NewCombined > MAX_int32)
			{
				Result = ETDShopResult::InvalidCount;
				break;
			}

			CombinedCount = static_cast<int32>(NewCombined);
			TotalCount64 += Line.Count;
			TotalPrice64 += static_cast<int64>(*UnitPrice) * Line.Count;

			if (TotalCount64 > MAX_int32 || TotalPrice64 > MAX_int32)
			{
				Result = ETDShopResult::InvalidCount;
				break;
			}
		}
	}

	UTDInventoryComponent* Inventory = ActiveInventory.Get();
	int32 SelectedKindCount = 0;
	if (Result == ETDShopResult::Success)
	{
		for (const TPair<FName, int32>& Pair : Counts)
		{
			const FTDItemRow* Definition =
				Inventory->FindItemDefinition(Pair.Key);
			if (!Definition)
			{
				Result = ETDShopResult::ItemNotSold;
				break;
			}

			// 포션 같은 묶음 상품은 수량과 관계없이 한 종류지만,
			// 장신구처럼 겹칠 수 없는 상품은 한 개가 한 종류를 차지합니다.
			if (Definition->bStackable && Pair.Value > MaxCount)
			{
				Result = ETDShopResult::InvalidCount;
				break;
			}

			SelectedKindCount += Definition->bStackable ? 1 : Pair.Value;
			if (SelectedKindCount > 4)
			{
				Result = ETDShopResult::InvalidCount;
				break;
			}
		}
	}

	if (Result == ETDShopResult::Success && Counts.IsEmpty())
	{
		Result = ETDShopResult::InvalidCount;
	}
	else if (Result == ETDShopResult::Success
		&& !Inventory->CanAfford(static_cast<int32>(TotalPrice64)))
	{
		Result = ETDShopResult::NotEnoughGold;
	}
	else if (Result == ETDShopResult::Success
		&& !Inventory->CanAddItems(Counts))
	{
		Result = ETDShopResult::InventoryFull;
	}

	if (Result == ETDShopResult::Success)
	{
		TotalCount = static_cast<int32>(TotalCount64);
		TotalPrice = static_cast<int32>(TotalPrice64);

		if (!Inventory->SpendGold(TotalPrice))
		{
			Result = ETDShopResult::NotEnoughGold;
		}
		else if (!Inventory->AddItems(Counts))
		{
			// 사전 공간 검사와 실제 지급은 같은 서버 게임 스레드에서 연속 실행됩니다.
			// 이 경로는 데이터가 실행 중 바뀐 예외 상황을 위한 최후의 환불입니다.
			Inventory->AddGold(TotalPrice);
			Result = ETDShopResult::InternalError;
			TotalPrice = 0;
		}
	}

	bProcessingRequest = false;

	if (ActiveSessionId != SessionId)
	{
		return;
	}

	const FString Message = Result == ETDShopResult::Success
		? FString::Printf(TEXT("%d개를 %d G에 구매했습니다."), TotalCount, TotalPrice)
		: ShopFailureMessage(Result);

	SendView(Message, RequestId, Result == ETDShopResult::Success);
	GetController()->ClientShopResult(Result, NAME_None, TotalCount, TotalPrice);
}

void UTDShopServiceComponent::ServerSellOne_Implementation(
	int32 SessionId,
	int64 ExpectedRevision,
	int32 SlotIndex,
	int32 RequestId)
{
	if (!CanProcessRequest(SessionId, ExpectedRevision, RequestId))
	{
		return;
	}

	bProcessingRequest = true;

	UTDInventoryComponent* Inventory = ActiveInventory.Get();
	const FTDItemInstance* Found = Inventory->FindBySlot(SlotIndex);
	const FName ItemId = Found ? Found->ItemId : NAME_None;
	FText DisplayName = FText::FromName(ItemId);

	if (const FTDItemRow* Definition = Inventory->FindItemDefinition(ItemId))
	{
		if (!Definition->DisplayName.IsEmpty())
		{
			DisplayName = Definition->DisplayName;
		}
	}

	ATDPlayerState* State =
		GetController()->GetPlayerState<ATDPlayerState>();

	int32 TotalPrice = 0;
	const ETDShopResult Result = State
		? UTDShopStatics::SellItem(State, ActiveShopId, SlotIndex, 1, TotalPrice)
		: ETDShopResult::NoCharacterSelected;

	bProcessingRequest = false;

	if (ActiveSessionId != SessionId)
	{
		return;
	}

	const FString Message = Result == ETDShopResult::Success
		? FString::Printf(
			TEXT("%s 1개를 %d G에 판매했습니다."),
			*DisplayName.ToString(),
			TotalPrice)
		: ShopFailureMessage(Result);

	SendView(Message, RequestId, false);
	GetController()->ClientShopResult(Result, ItemId, 1, TotalPrice);
}

void UTDShopServiceComponent::ServerSellCart_Implementation(
	int32 SessionId,
	int64 ExpectedRevision,
	const TArray<FTDShopSellLine>& Lines,
	int32 RequestId)
{
	if (!CanProcessRequest(SessionId, ExpectedRevision, RequestId))
	{
		return;
	}

	bProcessingRequest = true;

	UTDInventoryComponent* Inventory = ActiveInventory.Get();
	ETDShopResult Result = ETDShopResult::Success;
	TMap<int32, int32> CountsBySlot;

	// 화면은 네 종류까지만 담습니다. 서버도 같은 제한을 다시 검사해야
	// 수정된 클라이언트가 임의로 더 많은 줄을 보내지 못합니다.
	// 같은 아이템이 여러 인벤토리 묶음에 나뉘어 있으면 슬롯 줄은 네 개를
	// 넘을 수 있다. 줄 수가 아니라 아래에서 검사하는 고유 ItemId 수가
	// 네 종류 이하여야 한다.
	if (!Inventory || Lines.IsEmpty() || Lines.Num() > 128)
	{
		Result = ETDShopResult::InvalidCount;
	}

	if (Result == ETDShopResult::Success)
	{
		for (const FTDShopSellLine& Line : Lines)
		{
			if (Line.SlotIndex < 0 || Line.Count <= 0)
			{
				Result = ETDShopResult::InvalidCount;
				break;
			}

			int32& CombinedCount = CountsBySlot.FindOrAdd(Line.SlotIndex);
			const int64 NewCount =
				static_cast<int64>(CombinedCount) + Line.Count;

			if (NewCount > MAX_int32)
			{
				Result = ETDShopResult::InvalidCount;
				break;
			}

			CombinedCount = static_cast<int32>(NewCount);
		}
	}

	int64 TotalCount64 = 0;
	int64 TotalPrice64 = 0;
	TSet<FName> SelectedStackableTypes;
	int32 SelectedKindCount = 0;
	TMap<FName, int32> SelectedCountsByItem;
	const UTDShopSettings* ShopSettings = UTDShopSettings::Get();
	const int32 MaxCountPerItem = FMath::Max(
		1,
		ShopSettings ? ShopSettings->MaxCountPerTrade : 1);

	// 먼저 모든 줄을 검사합니다. 하나라도 팔 수 없으면 인벤토리를 전혀
	// 건드리지 않으므로 일부만 판매되는 상황이 생기지 않습니다.
	if (Result == ETDShopResult::Success)
	{
		for (const TPair<int32, int32>& Pair : CountsBySlot)
		{
			const FTDItemInstance* Item = Inventory->FindBySlot(Pair.Key);
			if (!Item || Item->Count < Pair.Value)
			{
				Result = ETDShopResult::ItemNotFound;
				break;
			}

			const FTDItemRow* Definition =
				Inventory->FindItemDefinition(Item->ItemId);
			if (!Definition || !Definition->bCanDiscard)
			{
				Result = ETDShopResult::ItemNotTradable;
				break;
			}

			if (Definition->bStackable)
			{
				if (!SelectedStackableTypes.Contains(Item->ItemId))
				{
					SelectedStackableTypes.Add(Item->ItemId);
					++SelectedKindCount;
				}
			}
			else
			{
				// 장신구 등 비스택 아이템은 ItemId가 같아도 개체마다
				// 강화·옵션이 다를 수 있으므로 각각 한 종류로 계산합니다.
				SelectedKindCount += Pair.Value;
			}

			if (SelectedKindCount > 4)
			{
				Result = ETDShopResult::InvalidCount;
				break;
			}

			if (Definition->bStackable)
			{
				int32& SelectedItemCount =
					SelectedCountsByItem.FindOrAdd(Item->ItemId);
				const int64 NewSelectedItemCount =
					static_cast<int64>(SelectedItemCount) + Pair.Value;
				if (NewSelectedItemCount > MaxCountPerItem)
				{
					Result = ETDShopResult::InvalidCount;
					break;
				}
				SelectedItemCount = static_cast<int32>(NewSelectedItemCount);
			}

			const int32 UnitPrice = UTDShopStatics::GetSellBackPrice(
				Inventory,
				ActiveShopId,
				Item->ItemId);

			TotalCount64 += Pair.Value;
			TotalPrice64 +=
				static_cast<int64>(UnitPrice) * Pair.Value;

			if (TotalCount64 > MAX_int32 || TotalPrice64 > MAX_int32)
			{
				Result = ETDShopResult::InvalidCount;
				break;
			}
		}
	}

	if (Result == ETDShopResult::Success && CountsBySlot.IsEmpty())
	{
		Result = ETDShopResult::InvalidCount;
	}

	if (Result == ETDShopResult::Success)
	{
		for (const TPair<int32, int32>& Pair : CountsBySlot)
		{
			if (!Inventory->RemoveItem(Pair.Key, Pair.Value))
			{
				// 위의 검증 직후 같은 서버 게임 스레드에서 처리하므로 정상적인
				// 실행에서는 도달하지 않는 데이터 오류 경로입니다.
				Result = ETDShopResult::InternalError;
				break;
			}
		}

		if (Result == ETDShopResult::Success && TotalPrice64 > 0)
		{
			if (!Inventory->AddGold(static_cast<int32>(TotalPrice64)))
			{
				Result = ETDShopResult::InternalError;
			}
		}
	}

	bProcessingRequest = false;

	if (ActiveSessionId != SessionId)
	{
		return;
	}

	const int32 TotalCount = Result == ETDShopResult::Success
		? static_cast<int32>(TotalCount64)
		: 0;
	const int32 TotalPrice = Result == ETDShopResult::Success
		? static_cast<int32>(TotalPrice64)
		: 0;

	const FString Message = Result == ETDShopResult::Success
		? FString::Printf(
			TEXT("%d개를 %d G에 판매했습니다."),
			TotalCount,
			TotalPrice)
		: ShopFailureMessage(Result);

	SendView(
		Message,
		RequestId,
		false,
		Result == ETDShopResult::Success);
	GetController()->ClientShopResult(
		Result,
		NAME_None,
		TotalCount,
		TotalPrice);
}

void UTDShopServiceComponent::EndService()
{
	ATDPlayerController* Controller = GetController();
	if (!Controller || !Controller->HasAuthority())
	{
		return;
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ServerTimer);
	}

	const int32 EndedSession = ActiveSessionId;

	ActiveSessionId = 0;
	ActiveNPC.Reset();
	ActiveShop.Reset();
	ActivePawn.Reset();
	ActiveInventory.Reset();
	StartZone = FGameplayTag();
	ActiveShopId = NAME_None;
	bClientOpened = false;
	bProcessingRequest = false;

	if (EndedSession > 0)
	{
		ClientCloseService(EndedSession);
	}
}

void UTDShopServiceComponent::ServerCloseService_Implementation(int32 SessionId)
{
	if (SessionId > 0 && SessionId == ActiveSessionId)
	{
		EndService();
	}
}

void UTDShopServiceComponent::ClientOpenService_Implementation(
	const FTDShopWindowView& View)
{
	ATDPlayerController* Controller = GetController();
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}

	CloseLocalWindow();
	LocalView = View;

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			LocalOpenTimer,
			this,
			&ThisClass::TryShowLocalWindow,
			0.1f,
			true);
	}

	TryShowLocalWindow();
}

void UTDShopServiceComponent::TryShowLocalWindow()
{
	ATDPlayerController* Controller = GetController();
	if (!Controller || LocalView.SessionId <= 0 || LocalWindow)
	{
		return;
	}

	const UTDInteractionFlowComponent* Flow =
		Controller->FindComponentByClass<UTDInteractionFlowComponent>();

	if (Flow && (Flow->IsDialogueActive()
		|| Flow->IsChapterPresentationActive()))
	{
		return;
	}

	ULocalPlayer* Player = Controller->GetLocalPlayer();
	UTDUIManagerSubsystem* Manager = Player
		? Player->GetSubsystem<UTDUIManagerSubsystem>()
		: nullptr;

	// BP_TDPlayerController를 다시 저장하지 않아도 지정된 상점 WBP를 사용합니다.
	// 에셋을 다른 위치로 옮기면 이 경로도 함께 바꿔야 합니다.
	if (ShopWindowClass == nullptr)
	{
		ShopWindowClass = LoadClass<UTDShopWindowWidget>(
			nullptr,
			TEXT("/Game/UI/InGame/WindowsLayer/Shop/WBP_ShopWindow.WBP_ShopWindow_C"));
	}

	if (ShopWindowClass == nullptr
		|| Manager == nullptr
		|| !Manager->IsGameplayWindowLayerReady())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("상점창을 열 수 없습니다. ShopWindowClass와 WBP_Root를 확인하세요."));

		const int32 FailedSession = LocalView.SessionId;
		CloseLocalWindow();
		ServerCloseService(FailedSession);
		return;
	}

	LocalWindow = CreateWidget<UTDShopWindowWidget>(
		Controller,
		ShopWindowClass);

	if (!LocalWindow)
	{
		const int32 FailedSession = LocalView.SessionId;
		CloseLocalWindow();
		ServerCloseService(FailedSession);
		return;
	}

	LocalWindow->InitializeService(this);

	if (!Manager->ShowServiceWindow(LocalWindow))
	{
		const int32 FailedSession = LocalView.SessionId;
		CloseLocalWindow(false);
		ServerCloseService(FailedSession);
		return;
	}

	LocalWindow->OnWindowClosed.AddUniqueDynamic(
		this,
		&ThisClass::HandleWindowClosed);

	LocalWindow->ApplyView(LocalView);

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LocalOpenTimer);
	}

	bCursorBeforeWindow = Controller->bShowMouseCursor;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	Controller->SetInputMode(InputMode);
	Controller->bShowMouseCursor = true;
}

void UTDShopServiceComponent::ClientUpdateService_Implementation(
	const FTDShopWindowView& View)
{
	if (View.SessionId != LocalView.SessionId || LocalView.SessionId <= 0)
	{
		return;
	}

	LocalView = View;
	if (LocalWindow)
	{
		LocalWindow->ApplyView(View);
	}
}

void UTDShopServiceComponent::ClientCloseService_Implementation(int32 SessionId)
{
	if (SessionId == LocalView.SessionId)
	{
		CloseLocalWindow();
	}
}

void UTDShopServiceComponent::HandleWindowClosed(
	UTDWindowBaseWidget* ClosedWindow)
{
	if (ClosedWindow != LocalWindow.Get())
	{
		return;
	}

	const int32 ClosedSession = LocalView.SessionId;
	CloseLocalWindow();
	ServerCloseService(ClosedSession);
}

void UTDShopServiceComponent::CloseLocalWindow(bool bRestoreInput)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LocalOpenTimer);
	}

	const bool bHadWindow = IsValid(LocalWindow);

	if (LocalWindow)
	{
		UTDShopWindowWidget* ClosingWindow = LocalWindow.Get();
		LocalWindow = nullptr;

		ClosingWindow->OnWindowClosed.RemoveDynamic(
			this,
			&ThisClass::HandleWindowClosed);

		ClosingWindow->RemoveFromParent();
	}

	LocalView = FTDShopWindowView();

	if (!bRestoreInput || !bHadWindow)
	{
		return;
	}

	ATDPlayerController* Controller = GetController();
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}

	ULocalPlayer* Player = Controller->GetLocalPlayer();
	UTDUIManagerSubsystem* Manager = Player
		? Player->GetSubsystem<UTDUIManagerSubsystem>()
		: nullptr;

	if (!Manager || !Manager->IsGameplayWindowLayerReady())
	{
		return;
	}

	const UTDInteractionFlowComponent* Flow =
		Controller->FindComponentByClass<UTDInteractionFlowComponent>();

	if (Flow && Flow->IsDialogueActive())
	{
		return;
	}

	const bool bKeepCursor =
		bCursorBeforeWindow || Manager->HasVisibleGameWindow();

	if (bKeepCursor)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		Controller->SetInputMode(InputMode);
	}
	else
	{
		Controller->SetInputMode(FInputModeGameOnly());
	}

	Controller->bShowMouseCursor = bKeepCursor;
}
