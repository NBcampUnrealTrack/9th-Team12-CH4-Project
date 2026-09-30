#include "Shop/TDShopStatics.h"

#include "Data/TDItemRow.h"
#include "Data/TDShopItemRow.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"
#include "Items/TDInventoryComponent.h"
#include "Player/TDPlayerState.h"
#include "Settings/TDShopSettings.h"
#include "Shop/TDShopComponent.h"

namespace
{
	const TCHAR* ShopContext = TEXT("UTDShopStatics::Shop");
	const TCHAR* ShopItemContext = TEXT("UTDShopStatics::ShopItem");

	const UDataTable* LoadShopTable()
	{
		const UTDShopSettings* Settings = UTDShopSettings::Get();
		return Settings ? Settings->ShopTable.LoadSynchronous() : nullptr;
	}

	const UDataTable* LoadShopItemTable()
	{
		const UTDShopSettings* Settings = UTDShopSettings::Get();
		return Settings ? Settings->ShopItemTable.LoadSynchronous() : nullptr;
	}
}

// ── 조회 ──────────────────────────────────────────────────

bool UTDShopStatics::GetShopInfo(FName ShopId, FTDShopRow& OutRow)
{
	const UDataTable* Table = LoadShopTable();
	if (Table == nullptr || ShopId.IsNone())
	{
		return false;
	}

	if (const FTDShopRow* Row = Table->FindRow<FTDShopRow>(ShopId, ShopContext, /*bWarnIfMissing=*/false))
	{
		OutRow = *Row;
		return true;
	}

	return false;
}

const FTDShopItemRow* UTDShopStatics::FindShopItem(FName ShopId, FName ItemId)
{
	const UDataTable* Table = LoadShopItemTable();
	if (Table == nullptr || ShopId.IsNone() || ItemId.IsNone())
	{
		return nullptr;
	}

	TArray<FTDShopItemRow*> Rows;
	Table->GetAllRows<FTDShopItemRow>(ShopItemContext, Rows);

	for (const FTDShopItemRow* Row : Rows)
	{
		if (Row != nullptr && Row->ShopId == ShopId && Row->ItemId == ItemId)
		{
			return Row;
		}
	}

	return nullptr;
}

float UTDShopStatics::GetSellBackRate(FName ShopId)
{
	FTDShopRow ShopRow;
	if (GetShopInfo(ShopId, ShopRow) && ShopRow.SellBackRateOverride > 0.f)
	{
		return ShopRow.SellBackRateOverride;
	}

	const UTDShopSettings* Settings = UTDShopSettings::Get();
	return Settings ? Settings->SellBackRate : 0.f;
}

int32 UTDShopStatics::GetSellBackPrice(const UTDInventoryComponent* Inventory,
	FName ShopId, FName ItemId)
{
	// 값은 아이템이 들고 있다. 상점 목록은 보지 않는다 — 그러면 몬스터 드롭을 팔 수 없다.
	const FTDItemRow* Definition = Inventory ? Inventory->FindItemDefinition(ItemId) : nullptr;
	if (Definition == nullptr)
	{
		return 0;
	}

	return FMath::RoundToInt(Definition->SellPrice * GetSellBackRate(ShopId));
}

TArray<FTDShopEntry> UTDShopStatics::GetShopEntries(const UTDInventoryComponent* Inventory,
	FName ShopId)
{
	TArray<FTDShopEntry> Result;

	const UDataTable* Table = LoadShopItemTable();
	if (Table == nullptr || ShopId.IsNone())
	{
		return Result;
	}

	// 비율은 한 번만 구한다. 행마다 다시 구하면 DT_Shop 을 매번 열게 된다.
	const float Rate = GetSellBackRate(ShopId);

	TArray<FTDShopItemRow*> Rows;
	Table->GetAllRows<FTDShopItemRow>(ShopItemContext, Rows);

	for (const FTDShopItemRow* Row : Rows)
	{
		if (Row == nullptr || Row->ShopId != ShopId)
		{
			continue;
		}

		FTDShopEntry& Entry = Result.AddDefaulted_GetRef();
		Entry.ItemId = Row->ItemId;
		Entry.Price = Row->Price;

		// 되팔 값을 서버가 미리 계산해 넣는다. 클라이언트가 다시 곱하면 반올림이 어긋나
		// 화면의 값과 실제로 들어오는 골드가 1 씩 달라진다.
		const FTDItemRow* Definition = Inventory ? Inventory->FindItemDefinition(Row->ItemId) : nullptr;
		Entry.SellBackPrice = Definition ? FMath::RoundToInt(Definition->SellPrice * Rate) : 0;
	}

	return Result;
}

// ── 근접 검증 ─────────────────────────────────────────────

bool UTDShopStatics::IsShopInRange(const ATDPlayerState* Player, FName ShopId)
{
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (Pawn == nullptr)
	{
		return false;
	}

	const UTDShopSettings* Settings = UTDShopSettings::Get();
	const float Range = Settings ? Settings->InteractRange : 0.f;
	const float RangeSquared = Range * Range;

	const UWorld* PlayerWorld = Pawn->GetWorld();
	const FVector PlayerLocation = Pawn->GetActorLocation();

	for (const TWeakObjectPtr<UTDShopComponent>& Weak : UTDShopComponent::GetAllShops())
	{
		const UTDShopComponent* Shop = Weak.Get();
		const AActor* ShopOwner = Shop ? Shop->GetOwner() : nullptr;

		if (ShopOwner == nullptr || Shop->GetShopId() != ShopId)
		{
			continue;
		}

		// PIE 는 한 프로세스에 서버·클라 월드가 함께 돈다. 걸러내지 않으면
		// 다른 창의 상인과 거리를 재게 된다.
		if (ShopOwner->GetWorld() != PlayerWorld)
		{
			continue;
		}

		if (FVector::DistSquared(PlayerLocation, ShopOwner->GetActorLocation()) <= RangeSquared)
		{
			return true;
		}
	}

	return false;
}

// ── 거래 ──────────────────────────────────────────────────

ETDShopResult UTDShopStatics::BuyItem(ATDPlayerState* Buyer, FName ShopId, FName ItemId,
	int32 Count, int32& OutTotalPrice)
{
	OutTotalPrice = 0;

	if (Buyer == nullptr || !Buyer->HasAuthority())
	{
		return ETDShopResult::InternalError;
	}

	UTDInventoryComponent* Inventory = Buyer->GetInventoryComponent();
	if (Inventory == nullptr)
	{
		return ETDShopResult::NoCharacterSelected;
	}

	const UTDShopSettings* Settings = UTDShopSettings::Get();
	const int32 MaxCount = Settings ? Settings->MaxCountPerTrade : 1;
	if (Count <= 0 || Count > MaxCount)
	{
		return ETDShopResult::InvalidCount;
	}

	FTDShopRow ShopRow;
	if (!GetShopInfo(ShopId, ShopRow))
	{
		return ETDShopResult::ShopNotFound;
	}

	if (!IsShopInRange(Buyer, ShopId))
	{
		return ETDShopResult::TooFar;
	}

	const FTDShopItemRow* ShopItem = FindShopItem(ShopId, ItemId);
	if (ShopItem == nullptr)
	{
		return ETDShopResult::ItemNotSold;
	}

	// int64 로 곱한다. Price 와 Count 가 각각 상한 근처면 int32 에서 넘쳐
	// 음수가 되고, 그러면 골드 검사를 그대로 통과한다.
	const int64 TotalPrice = static_cast<int64>(ShopItem->Price) * Count;
	if (TotalPrice > Inventory->GetGold())
	{
		return ETDShopResult::NotEnoughGold;
	}

	const int32 FinalPrice = static_cast<int32>(TotalPrice);

	// 골드를 먼저 빼고 아이템을 넣는다. 순서를 바꾸면 골드가 모자랄 때 넣은 아이템을
	// 도로 빼야 하는데, 그 사이에 인벤토리가 정렬되면 어느 칸인지 알 수 없다.
	if (!Inventory->SpendGold(FinalPrice))
	{
		return ETDShopResult::NotEnoughGold;
	}

	// AddItem 은 전부 들어가지 않으면 하나도 넣지 않는다. 부분 성공이 없으므로
	// 되돌릴 것은 골드뿐이다.
	if (!Inventory->AddItem(ItemId, Count))
	{
		Inventory->AddGold(FinalPrice);
		return ETDShopResult::InventoryFull;
	}

	OutTotalPrice = FinalPrice;
	return ETDShopResult::Success;
}

ETDShopResult UTDShopStatics::SellItem(ATDPlayerState* Seller, FName ShopId, int32 InventorySlot,
	int32 Count, int32& OutTotalPrice)
{
	OutTotalPrice = 0;

	if (Seller == nullptr || !Seller->HasAuthority())
	{
		return ETDShopResult::InternalError;
	}

	UTDInventoryComponent* Inventory = Seller->GetInventoryComponent();
	if (Inventory == nullptr)
	{
		return ETDShopResult::NoCharacterSelected;
	}

	if (Count <= 0)
	{
		return ETDShopResult::InvalidCount;
	}

	FTDShopRow ShopRow;
	if (!GetShopInfo(ShopId, ShopRow))
	{
		return ETDShopResult::ShopNotFound;
	}

	if (!IsShopInRange(Seller, ShopId))
	{
		return ETDShopResult::TooFar;
	}

	// 그 칸에 무엇이 있는지 찾는다. 배열 순서가 아니라 SlotIndex 로 찾아야 한다 —
	// 화면이 보는 자리와 배열 위치는 별개다(D26).
	const FTDItemInstance* Found = Inventory->GetItems().FindByPredicate(
		[InventorySlot](const FTDItemInstance& Item) { return Item.SlotIndex == InventorySlot; });

	if (Found == nullptr || Found->Count < Count)
	{
		return ETDShopResult::ItemNotFound;
	}

	// 반복자 뒤에서 배열이 바뀔 수 있으므로 필요한 값만 복사해 둔다.
	const FName ItemId = Found->ItemId;

	// 버릴 수 없는 것은 넘길 수도 없다(D90). RemoveItem 도 같은 조건으로 거부하지만,
	// 여기서 먼저 걸러야 "왜 안 되는지" 를 화면에 알려줄 수 있다.
	const FTDItemRow* Definition = Inventory->FindItemDefinition(ItemId);
	if (Definition == nullptr || !Definition->bCanDiscard)
	{
		return ETDShopResult::ItemNotTradable;
	}

	// 상점 목록에 있는지는 보지 않는다. 값은 아이템 자신이 들고 있고,
	// 0 이면 0 원에 팔린다 — 잡템을 비우는 통로가 필요하다.
	const int32 UnitPrice = GetSellBackPrice(Inventory, ShopId, ItemId);

	if (!Inventory->RemoveItem(InventorySlot, Count))
	{
		return ETDShopResult::InternalError;
	}

	const int32 TotalPrice = UnitPrice * Count;
	if (TotalPrice > 0)
	{
		Inventory->AddGold(TotalPrice);
	}

	OutTotalPrice = TotalPrice;
	return ETDShopResult::Success;
}
