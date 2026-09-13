#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Shop/TDShopView.h"
#include "TimerManager.h"
#include "TDShopServiceComponent.generated.h"

class APawn;
class ATDNPCBase;
class ATDPlayerController;
class UTDInventoryComponent;
class UTDShopComponent;
class UTDShopWindowWidget;
class UTDWindowBaseWidget;

/**
 * NPC 대화가 정상 종료된 동안에만 유효한 서버 권위 상점 세션입니다.
 * 클라이언트가 ShopId만 꾸며 보내 원격 구매하는 것을 막고, 거리 이탈 시 창도 닫습니다.
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDShopServiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDShopServiceComponent();

	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

	/** 서버의 정상 대화 종료 처리에서만 호출합니다. */
	void StartForNPC(ATDNPCBase* NPC);

	/** 새 대화 시작, 사망, 존 변경, 거리 초과 시 서버에서 종료합니다. */
	void EndService();

	/** 기존 단건 상점 RPC도 대화 세션 안에서만 허용하기 위한 검사입니다. */
	bool IsActiveForShop(FName ShopId) const;

	UFUNCTION(Server, Reliable)
	void ServerCloseService(int32 SessionId);

	/** 장바구니 전부를 검증한 뒤 한 번에 구매합니다. */
	UFUNCTION(Server, Reliable)
	void ServerBuyCart(
		int32 SessionId,
		int64 ExpectedRevision,
		const TArray<FTDShopCartLine>& Lines,
		int32 RequestId);

	/** 판매 목록에 담은 최대 4종류를 검증한 뒤 한 번에 판매합니다. */
	UFUNCTION(Server, Reliable)
	void ServerSellCart(
		int32 SessionId,
		int64 ExpectedRevision,
		const TArray<FTDShopSellLine>& Lines,
		int32 RequestId);

	/** 지정한 인벤토리 슬롯에서 정확히 한 개를 판매합니다. */
	UFUNCTION(Server, Reliable)
	void ServerSellOne(
		int32 SessionId,
		int64 ExpectedRevision,
		int32 SlotIndex,
		int32 RequestId);

protected:
	/** 비워 두면 /Game/UI/InGame/WindowsLayer/Shop/WBP_ShopWindow를 찾습니다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Shop")
	TSubclassOf<UTDShopWindowWidget> ShopWindowClass;

private:
	ATDPlayerController* GetController() const;
	UTDInventoryComponent* GetInventory() const;

	bool IsServerContextValid() const;
	bool CanProcessRequest(
		int32 SessionId,
		int64 ExpectedRevision,
		int32 RequestId);

	void TickServer();

	FTDShopWindowView MakeView(
		const FString& Message,
		int32 ReplyRequestId,
		bool bClearCart,
		bool bClearSellList = false);

	void SendView(
		const FString& Message = FString(),
		int32 ReplyRequestId = 0,
		bool bClearCart = false,
		bool bClearSellList = false);

	UFUNCTION(Client, Reliable)
	void ClientOpenService(const FTDShopWindowView& View);

	UFUNCTION(Client, Reliable)
	void ClientUpdateService(const FTDShopWindowView& View);

	UFUNCTION(Client, Reliable)
	void ClientCloseService(int32 SessionId);

	void TryShowLocalWindow();
	void CloseLocalWindow(bool bRestoreInput = true);

	UFUNCTION()
	void HandleWindowClosed(UTDWindowBaseWidget* ClosedWindow);

	TWeakObjectPtr<ATDNPCBase> ActiveNPC;
	TWeakObjectPtr<UTDShopComponent> ActiveShop;
	TWeakObjectPtr<APawn> ActivePawn;
	TWeakObjectPtr<UTDInventoryComponent> ActiveInventory;

	FGameplayTag StartZone;
	FName ActiveShopId;

	int32 SessionCounter = 0;
	int32 ActiveSessionId = 0;
	int32 LastRequestId = 0;

	int64 LastSentRevision = -1;
	int32 LastSentGold = -1;

	bool bClientOpened = false;
	bool bProcessingRequest = false;

	FTimerHandle ServerTimer;
	FTimerHandle LocalOpenTimer;

	UPROPERTY(Transient)
	FTDShopWindowView LocalView;

	UPROPERTY(Transient)
	TObjectPtr<UTDShopWindowWidget> LocalWindow;

	bool bCursorBeforeWindow = false;
};
