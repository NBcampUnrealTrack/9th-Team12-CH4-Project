#pragma once

#include "CoreMinimal.h"
#include "Market/TDMarketTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TDMarketSubsystem.generated.h"

class ATDPlayerState;

/**
 * 거래소. **서버에서만 의미가 있다.**
 *
 * GameInstance 서브시스템인 이유는 수명이다. 매물은 판매자가 접속해 있지 않아도
 * 살아 있어야 하므로 PlayerState 에 둘 수 없고, 맵이 바뀌어도 남아야 하므로
 * GameMode 나 World 도 맞지 않는다.
 *
 * ── 목록을 복제하지 않는다 ──
 * 매물이 수천 개가 될 수 있어 전체 복제는 성립하지 않는다. 클라이언트는 검색을
 * 요청하고 **그 시점의 스냅샷**을 받는다. 목록을 보는 사이 남이 사 갈 수 있고,
 * 그때는 구매가 ListingNotFound 로 거부된다 — 정상적으로 자주 일어나는 일이라
 * UI 는 그 결과를 받으면 목록을 새로 고치면 된다.
 *
 * ── 저장 ──
 * 지금은 메모리에만 있다. 서버를 껐다 켜면 매물이 사라진다. WriteSaveData /
 * ReadSaveData 자리를 열어 두었으니 DB 가 붙으면 그 둘만 채우면 된다.
 */
UCLASS()
class TD_PROJECT_API UTDMarketSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 인벤토리의 아이템을 매물로 올린다. **아이템은 인벤토리에서 빠진다.**
	 *
	 * 남겨 두면 등록해 놓고 그대로 착용하거나 버릴 수 있어 복제가 된다.
	 *
	 * @param Price          묶음 전체의 값. 낱개 가격이 아니다.
	 * @param OutListingId   성공했을 때 발급된 번호.
	 */
	ETDMarketResult ListItem(ATDPlayerState* Seller, int32 InventorySlot, int32 Price,
		int32& OutListingId);

	/**
	 * 매물을 산다. 묶음 통째로만 산다.
	 *
	 * 검사를 전부 마친 뒤에 실행을 시작한다 — 중간에 실패하면 골드만 빠지거나
	 * 아이템이 복제되기 때문이다.
	 */
	ETDMarketResult BuyListing(ATDPlayerState* Buyer, int32 ListingId);

	/** 자기 매물을 내린다. 아이템이 인벤토리로 돌아온다. */
	ETDMarketResult CancelListing(ATDPlayerState* Seller, int32 ListingId);

	/**
	 * 매물을 찾는다. **서버에서만 부른다** — 결과는 Client RPC 로 내려간다.
	 *
	 * @param ItemIdFilter  비우면 전부. 넣으면 그 아이템만.
	 * @param Page          0 부터. 한 쪽 크기는 UTDMarketSettings 가 정한다.
	 */
	TArray<FTDMarketListing> Search(FName ItemIdFilter, int32 Page) const;

	/** 이 사람이 올려 둔 매물. "내 판매 목록" 탭에 쓴다. */
	TArray<FTDMarketListing> GetListingsBySeller(const FString& SellerName) const;

	/**
	 * 오프라인일 때 팔려서 쌓인 대금을 지급한다. 없으면 아무 일도 하지 않는다.
	 *
	 * 접속(캐릭터 선택) 시점에 부른다. 우편함을 만들지 않고 처리하는 가장 단순한
	 * 방법이다 — 회수 UI 도 필요 없고 받으러 가는 동선도 없다.
	 *
	 * @return 실제로 지급한 금액. 0 이면 받을 것이 없었다.
	 */
	int32 ClaimPendingGold(ATDPlayerState* Player);

	/** 받을 돈이 얼마나 쌓여 있는지. UI 표시용. */
	int32 GetPendingGold(const FString& SellerName) const;

private:
	/** 접속 중인 사람을 이름으로 찾는다. 대금을 즉시 줄 수 있는지 판단할 때 쓴다. */
	ATDPlayerState* FindOnlinePlayerState(const FString& PlayerName) const;

	/** 대금을 지급한다. 접속 중이면 바로, 아니면 PendingGold 에 쌓는다. */
	void PaySeller(const FString& SellerName, int32 Amount);

	const FTDMarketListing* FindListing(int32 ListingId) const;

	/**
	 * 올라와 있는 매물 전부.
	 *
	 * 배열인 이유는 검색이 대부분 전체 순회이기 때문이다. 번호로 찾는 일은
	 * 구매·취소뿐이라 선형 탐색으로 충분하다. 수천 개를 넘어가면 그때
	 * 번호 → 인덱스 맵을 얹으면 된다.
	 */
	UPROPERTY()
	TArray<FTDMarketListing> Listings;

	/**
	 * 오프라인 판매자에게 줄 돈. 이름 → 금액.
	 *
	 * PlayerState 에 둘 수 없다 — 오프라인이면 그 객체가 없기 때문이다.
	 * 계정 시스템이 붙으면 키를 계정 ID 로 바꿔야 한다. 지금은 캐릭터 이름이
	 * 유일한 식별자라 **동명이인이 생기면 돈이 엉뚱한 사람에게 간다.**
	 */
	TMap<FString, int32> PendingGold;

	/** 다음에 발급할 번호. 0 은 "없음" 으로 쓰므로 1 부터 시작한다. */
	int32 NextListingId = 1;
};
