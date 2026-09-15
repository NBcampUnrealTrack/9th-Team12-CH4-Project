#pragma once

#include "CoreMinimal.h"
#include "Chat/TDChatTypes.h"
#include "Game/TDGameMode.h"
#include "Market/TDMarketTypes.h"
#include "Shop/TDShopTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Data/TDDialogueRow.h"
#include "Data/TDQuestTypes.h"
#include "TDPlayerController.generated.h"

class UTDInteractionFlowComponent;
class UTDShopServiceComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOnRespawnRequestResult, bool, bSucceeded);

/**
 * 존 이동이 거부됐을 때. 이 클라이언트에서만 불린다.
 *
 * UI 가 구독해 "24레벨부터 입장할 수 있습니다" 같은 안내를 띄운다.
 * 숫자는 DT_ZoneEnvironment 에서 읽으면 된다 — 클라이언트도 그 테이블을 갖고 있다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDOnZoneTravelFailed,
	FGameplayTag, TargetZoneId, ETDZoneTravelResult, Reason);

/**
 * 서버가 상호작용을 거부한 이유.
 */
UENUM(BlueprintType)
enum class ETDInteractionFailureReason : uint8
{
	None UMETA(DisplayName = "없음"),
	InventoryFull UMETA(DisplayName = "인벤토리 부족"),
	AlreadyClaimed UMETA(DisplayName = "이미 획득함"),
	QuestConditionNotMet UMETA(DisplayName = "퀘스트 조건 불일치"),
	InvalidDefinition UMETA(DisplayName = "데이터 설정 오류")
};

/**
 * 상호작용이 실패했을 때 해당 클라이언트에서 발생한다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FTDOnInteractionFailed,
	FName, ObjectId,
	ETDInteractionFailureReason, Reason);
//
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FTDOnDialogueLineReceived,
	int32, SessionId,
	FName, DialogueRow,
	FTDDialogueLineView, Line);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTDOnDialogueClosed,
	int32, SessionId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FTDOnQuestActionResult,
	FName, QuestId,
	ETDQuestActionResult, Result);

/**
 * 채팅이 도착했을 때. 이 클라이언트에서만 불린다.
 *
 * SenderName 이 비어 있으면 서버가 보낸 것이다(System·Loot). UI 는 그때
 * 이름을 그리지 않으면 된다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FTDOnChatReceived,
	ETDChatChannel, Channel,
	FString, SenderName,
	FString, Message);

/** 보낸 채팅이 거부됐을 때. 성공했을 때는 오지 않는다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTDOnChatSendFailed,
	ETDChatSendResult, Reason);

/**
 * 거래소 요청(등록·구매·취소)의 결과. **성공했을 때도 온다.**
 *
 * 채팅과 달리 성공을 알려야 한다 — 등록이 됐는지 안 됐는지는 목록을 다시
 * 받아보기 전까지 화면에서 알 수 없기 때문이다.
 *
 * ListingId 는 등록에 성공했을 때 발급된 번호다. 다른 경우에는 요청한 번호가
 * 그대로 돌아온다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FTDOnMarketResult,
	ETDMarketResult, Result,
	int32, ListingId);

/**
 * 거래소 검색 결과. **그 시점의 스냅샷이다.**
 *
 * 목록은 복제되지 않는다 — 매물이 수천 개가 될 수 있어 전체 복제가 성립하지
 * 않는다. 보는 사이 남이 사 가면 구매가 ListingNotFound 로 거부되므로,
 * UI 는 그때 다시 검색하면 된다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTDOnMarketSearchResult,
	const TArray<FTDMarketListing>&, Listings);

class ATDNPCBase;
class UDataTable;
class UTDZoneEnvironmentComponent;

/**
 * 플레이어 한 명의 의도를 나타내는 Controller.
 *
 * 서버와 그 플레이어의 클라이언트에만 존재한다. 다른 플레이어의 컨트롤러는 보이지 않는다.
 * 소유 관계가 명확해서 Server RPC 를 받기에 가장 안전한 자리이기도 하다.
 *
 * 지금은 접속 명령만 있다. Enhanced Input 바인딩과 CheatManager 가 나중에 여기 붙는다 —
 * 콘솔 명령(TD.*)도 GameMode 가 자리를 잡으면 CheatManager 로 옮길 수 있다.
 *
 * 이동 입력은 여기서 다루지 않는다. CharacterMovementComponent 가 예측과 서버 재현을
 * 이미 처리하므로 캐릭터 쪽에서 AddMovementInput 만 부르면 된다.
 */
UCLASS()
class TD_PROJECT_API ATDPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATDPlayerController();
	virtual void SetupInputComponent() override;

	/** Alt로 UI 조작용 커서와 게임 입력 모드 전환 */
	void ToggleMouseCursor();

    /** Shared keyboard route for gameplay input and focused HUD windows. */
    bool HandleNavShortcut(FKey Key);
    void OpenCharacterMenu();
    void OpenInventoryMenu();
    void OpenSkillMenu();
    void OpenQuestMenu();
    void OpenPartyMenu();
    void OpenSystemMenu();


	/**
	 * Pawn 을 새로 잡았을 때(접속·부활) 존 환경을 다시 적용한다.
	 *
	 * 카메라는 Pawn 에 딸려 있어 죽고 살아나면 BP 기본값으로 돌아온다. 존이 바뀌지
	 * 않았으니 OnZoneChanged 도 오지 않아, 여기서 부르지 않으면 그대로 남는다.
	 *
	 * 서버(리슨 호스트 포함)는 OnPossess, 클라이언트는 AcknowledgePossession 이 불린다.
	 * 실제로 카메라를 만지는 것은 로컬 컨트롤러뿐이라 양쪽 다 걸어 둔다.
	 */
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* InPawn) override;

	/**
	 * 개발용 접속 명령. 콘솔에 직접 입력한다.
	 *
	 *   TDConnect 127.0.0.1:7777
	 *   TDConnect 25.10.20.30:7777
	 */
	UFUNCTION(Exec)
	void TDConnect(const FString& Address);

	/**
	 * 이 플레이어 한 명만 다른 서버로 보낸다.
	 *
	 * 아직 호출하는 곳이 없다. 인스턴스 던전을 별도 서버로 띄우게 되면 그때 쓴다.
	 * ServerTravel 과 달리 접속자 전원이 아니라 대상 한 명만 이동한다.
	 */
	UFUNCTION(Client, Reliable)
	void ClientTravelToServer(const FString& Address);

	// ── 개발용 치트 ───────────────────────────────────────
	// 클라이언트에서 콘솔 명령을 쳤을 때 서버까지 전달하기 위한 통로다.
	//
	// 인벤토리 지급과 레벨 변경은 정상 경로에 RPC 를 두지 않았다 —
	// 열어두면 클라이언트가 스스로 아이템을 만들고 레벨을 올릴 수 있기 때문이다.
	//
	// UFUNCTION 은 전처리기 블록 안에 둘 수 없어서 선언은 항상 남는다.
	// 구현부를 UE_BUILD_SHIPPING 으로 막아 배포 빌드에서는 아무 일도 하지 않게 했다.
	// 선언까지 없애려면 CheatManager 로 옮겨야 한다 — 그쪽은 Shipping 에서 객체 자체가
	// 만들어지지 않으므로 더 확실하다. GameMode 가 자리를 잡았으니 나중에 이관할 것.

	UFUNCTION(Server, Reliable)
	void ServerDebugGiveItem(FName ItemId, int32 Count);

	/** 골드를 지급한다. 강화·거래소 테스트에 필요해 열어둔다. */
	UFUNCTION(Server, Reliable)
	void ServerDebugGiveGold(int32 Amount);

	UFUNCTION(Server, Reliable)
	void ServerDebugSetLevel(int32 NewLevel);

	UFUNCTION(Server, Reliable)
	void ServerDebugSetClass(FName NewClassId);

	/**
	 * 캐릭터 목록에 더미를 채운다. 세이브 담당이 붙으면 필요 없어진다.
	 *
	 * 목록을 채우는 것은 서버 권한인데 Play As Client 로 띄우면 서버 콘솔이 없으므로,
	 * 클라이언트 창에서도 테스트할 수 있게 통로를 연다.
	 */
	UFUNCTION(Server, Reliable)
	void ServerDebugGiveTestCharacters();

	/**
	 * 자기 캐릭터에게 고정 피해를 적용한다.
	 *
	 * ApplyRawDamage 는 서버 권한을 요구하므로 클라이언트 콘솔에서는 조용히 무시된다.
	 * 2인 PIE 의 클라이언트 창에서도 회복·사망을 테스트할 수 있도록 통로를 연다.
	 */
	UFUNCTION(Server, Reliable)
	void ServerDebugDamage(float Amount);

	/** 경험치를 지급한다. 레벨업 판정까지 서버에서 일어난다. */
	UFUNCTION(Server, Reliable)
	void ServerDebugAddExp(int32 Amount);

	/**
	 * 지정한 존으로 이동한다. 포탈이 없어도 존 이동을 검증할 수 있게 여는 통로다.
	 *
	 * 치트지만 **레벨 제한을 우회하지 않는다** — 서버 판정 자체를 테스트해야 하기 때문이다.
	 */
	UFUNCTION(Server, Reliable)
	void ServerDebugTravelToZone(FGameplayTag TargetZoneId, FName EntryName);

	/**
	 * 처치 경험치를 파티에 분배한다. 전투 쪽에 호출부가 붙기 전까지 검증용이다.
	 *
	 * AwardKillExp 가 서버 권한을 요구하므로 클라이언트 콘솔에서는 조용히 무시된다.
	 * 다른 치트와 같은 방식으로 통로를 연다.
	 */
	UFUNCTION(Server, Reliable)
	void ServerDebugPartyExp(int32 BaseAmount);

	/**
	 * 테스트 캐릭터 지급과 선택을 **한 RPC 로** 처리한다.
	 *
	 * 나눠 보내면 지급은 이쪽(PlayerController), 선택은 PlayerState 의 RPC 라
	 * 서로 다른 액터가 되어 도착 순서가 보장되지 않는다(§11-G).
	 * 뒤바뀌면 선택이 "목록이 비었다" 로 실패한다.
	 */
	UFUNCTION(Server, Reliable)
	void ServerDebugQuickStart(int32 SlotIndex);

	/**
	 * 지금 직업의 스킬을 전부 지정 레벨로 맞춘다.
	 *
	 * 액티브는 찍지 않으면 나가지 않아서, 스킬을 만질 때마다 TD.SkillUp 을 여러 번 치게 된다.
	 * TD.Start 도 안에서 같은 것을 부른다.
	 */
	UFUNCTION(Server, Reliable)
	void ServerDebugLearnSkills(int32 SkillLevel);

public:
	// ── 존 이동 피드백 ────────────────────────────────────

	/**
	 * 존 이동이 거부됐을 때 서버가 알려준다. **UI 담당이 구독할 지점이다.**
	 *
	 * 사유가 enum 인 이유는 문구를 UI 가 정해야 하기 때문이다. 서버가 완성된 문장을
	 * 보내면 현지화도 못 하고 화면 디자인이 서버 코드에 묶인다.
	 *
	 * 필요한 숫자(입장 레벨 등)는 UI 가 DT_ZoneEnvironment 에서 읽으면 된다 —
	 * 클라이언트도 그 테이블을 갖고 있다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "TD|World")
	FTDOnZoneTravelFailed OnZoneTravelFailed;

	/** 서버 전용 호출. 해당 클라이언트에게만 간다. */
	UFUNCTION(Client, Reliable)
	void ClientZoneTravelFailed(FGameplayTag TargetZoneId, ETDZoneTravelResult Reason);

	// ── 부활 ──────────────────────────────────────────────

	/**
	 * 부활 버튼이 부른다. **치트가 아니라 정식 경로**라 Shipping 에서도 살아 있다.
	 *
	 * 자동 부활 타이머와 같은 GameMode::RespawnPlayer 를 부른다. 둘이 갈라지면
	 * 한쪽만 고쳤을 때 "버튼으로는 되는데 타이머로는 안 되는" 상태가 된다.
	 *
	 * 살아 있는데 눌러도 서버가 거부하므로 클라이언트가 상태를 검사할 필요는 없다.
	 * 다만 UI 는 사망 중일 때만 버튼을 보여주는 편이 자연스럽다.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Combat")
	void ServerRequestRespawn();

	/** 수동 요청 결과는 요청한 소유 클라이언트에게만 보낸다. 생존 상태는 Pawn 복제를 따른다. */
	UFUNCTION(Client, Reliable)
	void ClientRespawnRequestResult(bool bSucceeded);

	UPROPERTY(BlueprintAssignable, Category = "TD|Combat")
	FTDOnRespawnRequestResult OnRespawnRequestResult;
	
	
	// ── 개인 상호작용 결과 ────────────────────────────────

	UFUNCTION(Client, Reliable)
	void ClientChestClaimed(
		FName ChestId,
		float DisappearDelay);

	UFUNCTION(Client, Reliable)
	void ClientInteractionFailed(
		FName ObjectId,
		ETDInteractionFailureReason Reason);

	UPROPERTY(BlueprintAssignable, Category = "TD|Interaction")
	FTDOnInteractionFailed OnInteractionFailed;

	
public:
	// ── NPC 대화 ──────────────────────────────────────────

	/**
	 * NPC의 Interact 함수가 서버에서 호출한다.
	 * 클라이언트가 직접 시작 NPC나 행을 지정할 수 없게 한다.
	 */
	void BeginDialogueFromNPC(
		ATDNPCBase* NPC,
		UDataTable* DialogueTable,
		FName StartRow);

	UFUNCTION(Server, Reliable, BlueprintCallable,
		Category = "TD|Dialogue")
	void ServerAdvanceDialogue(
		int32 SessionId,
		FName ExpectedCurrentRow);

	UFUNCTION(Server, Reliable, BlueprintCallable,
		Category = "TD|Dialogue")
	void ServerCancelDialogue(int32 SessionId);

	UFUNCTION(Client, Reliable)
	void ClientShowDialogueLine(
		int32 SessionId,
		FName DialogueRow,
		FTDDialogueLineView Line);

	UFUNCTION(Client, Reliable)
	void ClientCloseDialogue(int32 SessionId);

	UFUNCTION(Client, Reliable)
	void ClientQuestActionResult(
		FName QuestId,
		ETDQuestActionResult Result);

	UPROPERTY(BlueprintAssignable, Category = "TD|Dialogue")
	FTDOnDialogueLineReceived OnDialogueLineReceived;

	UPROPERTY(BlueprintAssignable, Category = "TD|Dialogue")
	FTDOnDialogueClosed OnDialogueClosed;

	UPROPERTY(BlueprintAssignable, Category = "TD|Quest")
	FTDOnQuestActionResult OnQuestActionResult;

public:
	// ══════════════════════════════════════════════════════
	//  채팅
	// ══════════════════════════════════════════════════════

	/**
	 * 채팅을 보낸다. **UI 의 입력창이 부르는 함수다.**
	 *
	 * 검열·길이 제한·도배 방지는 전부 서버가 한다. 클라이언트에서 걸러 봐야
	 * 우회되므로 UI 는 아무것도 검사하지 않고 그대로 넘기면 된다.
	 *
	 * @param TargetName  귓속말 대상의 이름. 다른 채널에서는 무시한다.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Chat")
	void ServerSendChat(ETDChatChannel Channel, const FString& Message, const FString& TargetName);

	/** 서버가 보내온 채팅. 이 클라이언트에게만 온다. */
	UFUNCTION(Client, Reliable)
	void ClientReceiveChat(ETDChatChannel Channel, const FString& SenderName, const FString& Message);

	/** 채팅이 도착했을 때. **UI 가 구독할 지점이다.** */
	UPROPERTY(BlueprintAssignable, Category = "TD|Chat")
	FTDOnChatReceived OnChatReceived;

	/**
	 * 보낸 채팅이 거부됐을 때. 보낸 사람에게만 온다.
	 *
	 * 성공했을 때는 오지 않는다 — 자기 말이 채팅창에 뜨는 것이 곧 성공 신호라
	 * 따로 알릴 필요가 없다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "TD|Chat")
	FTDOnChatSendFailed OnChatSendFailed;

	UFUNCTION(Client, Reliable)
	void ClientChatSendFailed(ETDChatSendResult Reason);

	// ══════════════════════════════════════════════════════
	//  거래소
	// ══════════════════════════════════════════════════════
	// 서버 서브시스템(UTDMarketSubsystem)이 실제 처리를 한다. 여기는 통로다.

	/**
	 * 인벤토리의 아이템을 매물로 올린다. **아이템이 인벤토리에서 빠진다.**
	 *
	 * @param Price  묶음 전체의 값. 낱개 가격이 아니다 — 부분 구매가 없다.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Market")
	void ServerListItem(int32 InventorySlot, int32 Price);

	/** 매물을 산다. 묶음 통째로만 산다. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Market")
	void ServerBuyListing(int32 ListingId);

	/** 자기 매물을 내린다. 아이템이 인벤토리로 돌아온다. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Market")
	void ServerCancelListing(int32 ListingId);

	/**
	 * 매물을 찾는다. 결과는 OnMarketSearchResult 로 돌아온다.
	 *
	 * @param ItemIdFilter  비우면 전부. 넣으면 그 아이템만.
	 * @param Page          0 부터. 한 쪽 크기는 프로젝트 세팅에서 정한다.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Market")
	void ServerSearchListings(FName ItemIdFilter, int32 Page);

	/** 자기가 올려 둔 매물을 받는다. "내 판매 목록" 탭에 쓴다. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Market")
	void ServerRequestMyListings();

	UFUNCTION(Client, Reliable)
	void ClientMarketResult(ETDMarketResult Result, int32 ListingId);

	UFUNCTION(Client, Reliable)
	void ClientMarketSearchResult(const TArray<FTDMarketListing>& Listings);

	/** 등록·구매·취소의 결과. **UI 가 구독할 지점이다.** */
	UPROPERTY(BlueprintAssignable, Category = "TD|Market")
	FTDOnMarketResult OnMarketResult;

	// ══════════════════════════════════════════════════════
	//  NPC 상점
	// ══════════════════════════════════════════════════════
	// 거래소와 다른 시스템이다 — 플레이어 간이 아니라 NPC 가 고정 목록을 판다.
	// 실제 처리는 UTDShopStatics 가 하고 여기는 통로다.
	//
	// **목록 조회는 여기 없다.** 재고가 무제한이고 가격이 테이블이라 클라이언트가
	// UTDShopStatics::GetShopEntries 로 직접 읽으면 된다. RPC 를 왕복할 이유가 없다.

	/**
	 * 상점에서 산다. 사거리 검증은 서버가 한다.
	 *
	 * @param ShopId  UTDShopComponent 에 적힌 값. 어느 상인 앞인지 클라이언트가 알려주고,
	 *                정말 그 앞에 있는지는 서버가 확인한다.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Shop")
	void ServerBuyFromShop(FName ShopId, FName ItemId, int32 Count);

	/** 상점에 판다. **그 상점이 파는 물건만** 사 준다. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Shop")
	void ServerSellToShop(FName ShopId, int32 InventorySlot, int32 Count);

	UFUNCTION(Client, Reliable)
	void ClientShopResult(ETDShopResult Result, FName ItemId, int32 Count, int32 TotalPrice);

	/** 사거나 판 결과. **UI 가 구독할 지점이다.** */
	UPROPERTY(BlueprintAssignable, Category = "TD|Shop")
	FTDOnShopResult OnShopResult;

	/** 검색 결과. 내 판매 목록도 같은 델리게이트로 온다. */
	UPROPERTY(BlueprintAssignable, Category = "TD|Market")
	FTDOnMarketSearchResult OnMarketSearchResult;

private:
	/**
	 * 존이 바뀔 때 카메라·라이팅을 맞춘다. 로컬 컨트롤러에서만 실제로 동작한다.
	 *
	 * 컨트롤러에 둔 이유는 카메라와 존을 둘 다 아는 자리가 여기뿐이어서다 —
	 * 카메라는 Pawn 에 있고 존은 PlayerState 에 있다.
	 */
	UPROPERTY(VisibleAnywhere, Category = "TD|World")
	TObjectPtr<UTDZoneEnvironmentComponent> ZoneEnvironmentComponent;

	/**
	 * 마지막으로 채팅을 보낸 시각. 도배 방지에 쓴다. **서버에서만 의미가 있다.**
	 *
	 * 요청에 시각을 담게 하지 않고 서버가 직접 잰다. 클라이언트가 보낸 시각을
	 * 믿으면 그대로 조작된다.
	 */
	double LastChatSendTime = 0.0;

	void SendCurrentDialogueLine();
	void EndDialogueSession();
	bool ApplyDialogueAction(
		const FTDDialogueAction& Action);

	UPROPERTY(Transient)
	TObjectPtr<ATDNPCBase> ActiveDialogueNPC;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ActiveDialogueTable;

	FName ActiveDialogueRow;
	int32 ActiveDialogueSessionId = 0;
	int32 DialogueSessionCounter = 0;
public:
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="TD|UI|Account")
    void ServerLogin(const FString& LoginId, const FString& Password);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="TD|UI|Account")
    void ServerRegisterAccount(const FString& LoginId, const FString& Password);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="TD|UI|Account")
    void ServerCreateCharacter(const FString& Name, FName ClassId);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="TD|UI|Account")
    void ServerDeleteCharacter(const FGuid& CharacterId);
    UFUNCTION(Client, Reliable)
    void ClientAccountActionResult(FName Action, bool bSuccess, const FString& Message);
    int32 GetAccountReplySerial() const { return AccountReplySerial; }
    FName GetAccountReplyAction() const { return AccountReplyAction; }
    bool WasAccountActionSuccessful() const { return bAccountActionSuccessful; }
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="TD|UI|Account")
    void ServerSelectCharacter(int32 SlotIndex);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="TD|UI|Account")
    void ServerSaveCharacter();
    /** 서버의 명시적 저장 요청. 백엔드 모드의 true는 접수이며 완료는 비동기 통지된다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="TD|UI|Account")
    bool SaveCharacter();
    UFUNCTION(BlueprintPure, Category="TD|UI|Account")
    bool IsLoggedIn() const { return DummyAccountId.IsValid(); }
    UFUNCTION(BlueprintPure, Category="TD|UI|Account")
    FString GetAccountMessage() const { return DummyMessage; }
    UFUNCTION(Client, Reliable)
    void ClientAccountMessage(const FString& Message);
    UFUNCTION(Exec, BlueprintCallable, Category="TD|UI|Account")
    void TDLoginScreen();
    UFUNCTION(Exec)
    void TDSave();
    UFUNCTION(Exec, BlueprintCallable, Category="TD|UI|Account")
    void TDCharacterSelect();
    UFUNCTION(Exec, BlueprintCallable, Category="TD|UI|Account")
    void TDLogout();
    UFUNCTION(Server, Reliable)
    void ServerLeaveCharacter(bool bLogout);

protected:
    UPROPERTY(EditDefaultsOnly, Category="TD|UI|Account")
    bool bStartAccountFlowOnBeginPlay = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Account")
    TSubclassOf<class UTDLoginWidget> DummyLoginWidgetClass;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Destroyed() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    UPROPERTY(Replicated) FGuid DummyAccountId;
    friend class UTDBackendSaveSubsystem;
    friend class FTDBackendControllerTest;
    bool CaptureAccountData(struct FTDPlayerSaveData& Data) const;
    void ApplyAccountRecord(const struct FTDDummyCharacterRecord& Record,int32 SlotIndex);
    void FinishBackendLeave(const TArray<struct FTDCharacterSummary>& Characters,bool bLogout);
    FGuid DummyCharacterId;
    FString DummyMessage;
    int32 AccountReplySerial = 0;
    FName AccountReplyAction;
    bool bAccountActionSuccessful = false;
    double NextAccountMutationTime = 0.0;


public:
    UFUNCTION(BlueprintPure, Category="TD|Interaction")
    UTDInteractionFlowComponent* GetInteractionFlowComponent() const { return InteractionFlowComponent; }
    UFUNCTION(BlueprintPure, Category="TD|Shop")
    UTDShopServiceComponent* GetShopServiceComponent() const { return ShopServiceComponent; }
private:
    UPROPERTY(VisibleAnywhere, Category="TD|Interaction")
    TObjectPtr<UTDInteractionFlowComponent> InteractionFlowComponent;
    UPROPERTY(VisibleAnywhere, Category="TD|Shop")
    TObjectPtr<UTDShopServiceComponent> ShopServiceComponent;
};

