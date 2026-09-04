#include "Market/TDMarketSubsystem.h"

#include "Data/TDItemRow.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Items/TDInventoryComponent.h"
#include "Player/TDPlayerState.h"
#include "Settings/TDMarketSettings.h"

ATDPlayerState* UTDMarketSubsystem::FindOnlinePlayerState(const FString& PlayerName) const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* State = World ? World->GetGameState() : nullptr;

	if (State == nullptr || PlayerName.IsEmpty())
	{
		return nullptr;
	}

	for (APlayerState* PlayerState : State->PlayerArray)
	{
		if (PlayerState != nullptr && PlayerState->GetPlayerName() == PlayerName)
		{
			return Cast<ATDPlayerState>(PlayerState);
		}
	}

	return nullptr;
}

void UTDMarketSubsystem::PaySeller(const FString& SellerName, int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	if (ATDPlayerState* Seller = FindOnlinePlayerState(SellerName))
	{
		if (UTDInventoryComponent* Inventory = Seller->GetInventoryComponent())
		{
			Inventory->AddGold(Amount);
			return;
		}
	}

	// 접속 중이 아니면 쌓아 둔다. 다음 접속 때 ClaimPendingGold 가 넘겨준다.
	PendingGold.FindOrAdd(SellerName) += Amount;

	UE_LOG(LogTemp, Log, TEXT("거래소: '%s' 는 접속 중이 아니라 %d 골드를 보관한다 (누적 %d)"),
		*SellerName, Amount, PendingGold[SellerName]);
}

int32 UTDMarketSubsystem::GetPendingGold(const FString& SellerName) const
{
	const int32* Found = PendingGold.Find(SellerName);
	return Found != nullptr ? *Found : 0;
}

int32 UTDMarketSubsystem::ClaimPendingGold(ATDPlayerState* Player)
{
	if (Player == nullptr || !Player->HasAuthority())
	{
		return 0;
	}

	const FString PlayerName = Player->GetPlayerName();

	int32 Amount = 0;
	if (!PendingGold.RemoveAndCopyValue(PlayerName, Amount) || Amount <= 0)
	{
		return 0;
	}

	UTDInventoryComponent* Inventory = Player->GetInventoryComponent();
	if (Inventory == nullptr)
	{
		// 지급할 수 없으면 도로 넣어 둔다. 삼키면 그대로 사라진다.
		PendingGold.Add(PlayerName, Amount);
		return 0;
	}

	Inventory->AddGold(Amount);

	UE_LOG(LogTemp, Log, TEXT("거래소: '%s' 에게 보관 중이던 %d 골드를 지급했다."),
		*PlayerName, Amount);

	return Amount;
}

const FTDMarketListing* UTDMarketSubsystem::FindListing(int32 ListingId) const
{
	return Listings.FindByPredicate([ListingId](const FTDMarketListing& Entry)
	{
		return Entry.ListingId == ListingId;
	});
}

ETDMarketResult UTDMarketSubsystem::ListItem(ATDPlayerState* Seller, int32 InventorySlot,
	int32 Price, int32& OutListingId)
{
	OutListingId = 0;

	if (Seller == nullptr || !Seller->HasAuthority())
	{
		return ETDMarketResult::InternalError;
	}

	// 캐릭터를 고르기 전에는 이름이 접속 ID("mpc-3B8A...") 다. 그 이름으로 매물을
	// 올리면 나중에 판매자를 찾을 수 없어 대금을 줄 방법이 없어진다.
	if (!Seller->HasSelectedCharacter())
	{
		return ETDMarketResult::NoCharacterSelected;
	}

	UTDInventoryComponent* Inventory = Seller->GetInventoryComponent();
	if (Inventory == nullptr)
	{
		return ETDMarketResult::InternalError;
	}

	const UTDMarketSettings* Settings = UTDMarketSettings::Get();
	const int32 MaxPrice = Settings ? Settings->MaxPrice : 100000000;
	const int32 MaxListings = Settings ? Settings->MaxListingsPerPlayer : 10;

	if (Price <= 0 || Price > MaxPrice)
	{
		return ETDMarketResult::InvalidPrice;
	}

	const FString SellerName = Seller->GetPlayerName();
	if (GetListingsBySeller(SellerName).Num() >= MaxListings)
	{
		return ETDMarketResult::TooManyListings;
	}

	const FTDItemInstance* Item = Inventory->FindBySlot(InventorySlot);
	if (Item == nullptr)
	{
		return ETDMarketResult::ItemNotFound;
	}

	// 거래 가능 여부를 따로 두지 않고 bCanDiscard 를 쓴다. 버릴 수 없는 것(퀘스트
	// 아이템)은 남에게 넘길 수도 없어야 하므로 지금은 같은 조건으로 충분하다.
	// "버릴 수는 있지만 팔 수는 없는" 물건이 생기면 그때 bTradable 을 추가한다.
	const FTDItemRow* Row = Inventory->FindItemDefinition(Item->ItemId);
	if (Row != nullptr && !Row->bCanDiscard)
	{
		return ETDMarketResult::ItemNotTradable;
	}

	// 여기서부터 실행. 위 검사를 전부 통과했으므로 실패하지 않는다.
	FTDItemInstance TakenItem;
	if (!Inventory->TakeItemAt(InventorySlot, TakenItem))
	{
		return ETDMarketResult::ItemNotFound;
	}

	FTDMarketListing& NewListing = Listings.AddDefaulted_GetRef();
	NewListing.ListingId = NextListingId++;
	NewListing.SellerName = SellerName;
	NewListing.Item = TakenItem;
	NewListing.Price = Price;

	OutListingId = NewListing.ListingId;

	UE_LOG(LogTemp, Log, TEXT("거래소 등록: [%d] '%s' x%d — %d 골드 (판매자 %s)"),
		NewListing.ListingId, *TakenItem.ItemId.ToString(), TakenItem.Count, Price, *SellerName);

	return ETDMarketResult::Success;
}

ETDMarketResult UTDMarketSubsystem::BuyListing(ATDPlayerState* Buyer, int32 ListingId)
{
	if (Buyer == nullptr || !Buyer->HasAuthority())
	{
		return ETDMarketResult::InternalError;
	}

	// 아직 게임에 들어오지 않은 상태다. 캐릭터 선택 화면에서 거래가 되면
	// 자기 물건인지 판정하는 이름 비교도 성립하지 않는다.
	if (!Buyer->HasSelectedCharacter())
	{
		return ETDMarketResult::NoCharacterSelected;
	}

	UTDInventoryComponent* Inventory = Buyer->GetInventoryComponent();
	if (Inventory == nullptr)
	{
		return ETDMarketResult::InternalError;
	}

	const int32 Index = Listings.IndexOfByPredicate([ListingId](const FTDMarketListing& Entry)
	{
		return Entry.ListingId == ListingId;
	});

	if (Index == INDEX_NONE)
	{
		// 목록을 보는 사이 남이 사 간 경우다. 오류가 아니라 흔한 상황이다.
		return ETDMarketResult::ListingNotFound;
	}

	// ── 1단계: 검사만. 하나라도 걸리면 아무것도 바꾸지 않는다 ──
	// 순서를 이렇게 나눈 이유는 중간에 실패했을 때 돌이킬 방법이 없어서다.
	// 골드를 먼저 빼고 아이템을 넣다 실패하면 돈만 사라진다.
	const FTDMarketListing Listing = Listings[Index];

	if (Listing.SellerName == Buyer->GetPlayerName())
	{
		// 자기 물건을 사면 수수료만큼 손해다. 취소를 쓰면 된다.
		return ETDMarketResult::CannotBuyOwnListing;
	}

	if (!Inventory->CanAfford(Listing.Price))
	{
		return ETDMarketResult::NotEnoughGold;
	}

	if (Inventory->GetUsedSlotCount() >= Inventory->GetSlotCapacity())
	{
		// 산 물건을 넣을 자리가 없다. 골드는 아직 빠지지 않았다.
		return ETDMarketResult::InventoryFull;
	}

	// ── 2단계: 실행 ──
	if (!Inventory->SpendGold(Listing.Price))
	{
		// 위에서 확인했으므로 도달하지 않는다. 도달했다면 계산이 틀린 것이다.
		UE_LOG(LogTemp, Error, TEXT("거래소: 골드 검사를 통과하고도 차감에 실패했다. [%d]"), ListingId);
		return ETDMarketResult::InternalError;
	}

	if (!Inventory->PutItemInFirstEmptySlot(Listing.Item))
	{
		// 마찬가지로 도달하지 않아야 한다. 도달했다면 빈 칸을 되돌려 준다 —
		// 물건 없이 돈만 잃는 것이 가장 나쁘다.
		UE_LOG(LogTemp, Error, TEXT("거래소: 빈 칸 검사를 통과하고도 지급에 실패했다. [%d]"), ListingId);
		Inventory->AddGold(Listing.Price);
		return ETDMarketResult::InternalError;
	}

	// 수수료는 판매자 대금에서 뗀다. 떼인 만큼은 사라진다 — 골드가 쌓이기만 하면
	// 물가가 오르므로 빠져나가는 구멍이 필요하다.
	const UTDMarketSettings* Settings = UTDMarketSettings::Get();
	const float FeeRate = Settings ? Settings->SaleFeeRate : 0.05f;

	const int32 Fee = FMath::FloorToInt(Listing.Price * FeeRate);
	const int32 SellerGain = FMath::Max(0, Listing.Price - Fee);

	PaySeller(Listing.SellerName, SellerGain);

	Listings.RemoveAt(Index);

	UE_LOG(LogTemp, Log, TEXT("거래소 판매: [%d] '%s' — %s 가 %d 골드에 삼 (판매자 %s 수령 %d, 수수료 %d)"),
		ListingId, *Listing.Item.ItemId.ToString(), *Buyer->GetPlayerName(),
		Listing.Price, *Listing.SellerName, SellerGain, Fee);

	return ETDMarketResult::Success;
}

ETDMarketResult UTDMarketSubsystem::CancelListing(ATDPlayerState* Seller, int32 ListingId)
{
	if (Seller == nullptr || !Seller->HasAuthority())
	{
		return ETDMarketResult::InternalError;
	}

	// 이름이 접속 ID 인 상태로는 판매자 대조가 성립하지 않는다.
	if (!Seller->HasSelectedCharacter())
	{
		return ETDMarketResult::NoCharacterSelected;
	}

	UTDInventoryComponent* Inventory = Seller->GetInventoryComponent();
	if (Inventory == nullptr)
	{
		return ETDMarketResult::InternalError;
	}

	const int32 Index = Listings.IndexOfByPredicate([ListingId](const FTDMarketListing& Entry)
	{
		return Entry.ListingId == ListingId;
	});

	if (Index == INDEX_NONE)
	{
		return ETDMarketResult::ListingNotFound;
	}

	if (Listings[Index].SellerName != Seller->GetPlayerName())
	{
		return ETDMarketResult::NotSeller;
	}

	// 돌려받을 자리가 없으면 매물을 그대로 둔다. 내렸는데 물건이 사라지는 것보다
	// "가방을 비우고 다시 시도" 가 낫다.
	if (Inventory->GetUsedSlotCount() >= Inventory->GetSlotCapacity())
	{
		return ETDMarketResult::InventoryFull;
	}

	if (!Inventory->PutItemInFirstEmptySlot(Listings[Index].Item))
	{
		return ETDMarketResult::InternalError;
	}

	UE_LOG(LogTemp, Log, TEXT("거래소 취소: [%d] '%s' (판매자 %s)"),
		ListingId, *Listings[Index].Item.ItemId.ToString(), *Listings[Index].SellerName);

	Listings.RemoveAt(Index);

	return ETDMarketResult::Success;
}

TArray<FTDMarketListing> UTDMarketSubsystem::Search(FName ItemIdFilter, int32 Page) const
{
	const UTDMarketSettings* Settings = UTDMarketSettings::Get();
	const int32 PageSize = Settings ? Settings->SearchPageSize : 50;

	const int32 StartIndex = FMath::Max(0, Page) * PageSize;

	TArray<FTDMarketListing> Result;

	int32 Matched = 0;
	for (const FTDMarketListing& Entry : Listings)
	{
		if (!ItemIdFilter.IsNone() && Entry.Item.ItemId != ItemIdFilter)
		{
			continue;
		}

		// 앞쪽 페이지에 해당하는 것은 세기만 하고 넘긴다.
		if (Matched++ < StartIndex)
		{
			continue;
		}

		Result.Add(Entry);

		if (Result.Num() >= PageSize)
		{
			break;
		}
	}

	return Result;
}

TArray<FTDMarketListing> UTDMarketSubsystem::GetListingsBySeller(const FString& SellerName) const
{
	TArray<FTDMarketListing> Result;

	for (const FTDMarketListing& Entry : Listings)
	{
		if (Entry.SellerName == SellerName)
		{
			Result.Add(Entry);
		}
	}

	return Result;
}
