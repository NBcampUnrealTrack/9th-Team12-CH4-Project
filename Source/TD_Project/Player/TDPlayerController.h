#pragma once

#include "CoreMinimal.h"
#include "Chat/TDChatTypes.h"
#include "Game/TDGameMode.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Data/TDDialogueRow.h"
#include "Data/TDQuestTypes.h"
#include "TDPlayerController.generated.h"

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

class ATDNPCBase;
class UDataTable;

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

private:
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
};

