#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Option/TDOptionView.h"
#include "TimerManager.h"
#include "TDOptionServiceComponent.generated.h"

class APlayerController;
class APawn;
class ATDNPCBase;
class UTDInventoryComponent;
class UTDOptionWindowWidget;
class UTDWindowBaseWidget;

/**
 * 추가 옵션(잠재능력) 재설정 NPC의 서버 진행. **강화(UTDEnhanceServiceComponent)와 같은 구조다.**
 *
 * 대화가 정상적으로 끝나면 시작하고, 거리·존·사망·새 대화로 끝난다. 화면에 필요한 값은
 * 전부 FTDOptionWindowView 하나로 묶어 보낸다 — 창이 인벤토리를 직접 뒤지면
 * 서버가 거절한 요청도 클라이언트 화면에서는 성공한 것처럼 보이게 만들 수 있다.
 *
 * 굴리기 자체는 UTDItemUseComponent::RerollOptionsForService 가 한다. 이 컴포넌트는
 * 세션이 유효한지만 판단하고, 골드·확률·테이블은 그쪽 규칙을 그대로 쓴다.
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDOptionServiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDOptionServiceComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 서버의 정상 대화 종료 처리에서만 호출합니다. */
	void StartForNPC(ATDNPCBase* NPC);

	/** 새 대화 시작, 거리 초과 등에 의해 서버에서 종료합니다. */
	void EndService();

	UFUNCTION(Server, Reliable)
	void ServerCloseService(int32 SessionId);

	UFUNCTION(Server, Reliable)
	void ServerTryReroll(
		int32 SessionId,
		int64 ExpectedRevision,
		int32 SlotIndex,
		int32 RequestId);

private:
	APlayerController* GetController() const;
	UTDInventoryComponent* GetInventory() const;
	class UTDItemUseComponent* GetItemUse() const;

	bool IsServerContextValid() const;
	void TickServer();

	FTDOptionWindowView MakeView(
		const FString& Message,
		int32 ReplyRequestId,
		bool bResetSelection);

	void SendView(
		const FString& Message = FString(),
		int32 ReplyRequestId = 0,
		bool bResetSelection = true);

	UFUNCTION(Client, Reliable)
	void ClientOpenService(const FTDOptionWindowView& View);

	UFUNCTION(Client, Reliable)
	void ClientUpdateService(const FTDOptionWindowView& View);

	UFUNCTION(Client, Reliable)
	void ClientCloseService(int32 SessionId);

	void TryShowLocalWindow();
	void CloseLocalWindow(bool bRestoreInput = true);

	UFUNCTION()
	void HandleWindowClosed(UTDWindowBaseWidget* ClosedWindow);

	TWeakObjectPtr<ATDNPCBase> ActiveNPC;
	TWeakObjectPtr<APawn> ActivePawn;
	TWeakObjectPtr<UTDInventoryComponent> ActiveInventory;

	FGameplayTag StartZone;

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
	FTDOptionWindowView LocalView;

	UPROPERTY(Transient)
	TObjectPtr<UTDOptionWindowWidget> LocalWindow;

	bool bCursorBeforeWindow = false;
};
