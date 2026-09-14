#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Enhance/TDEnhanceView.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "TDEnhanceServiceComponent.generated.h"

class APlayerController;
class APawn;
class ATDNPCBase;
class UTDInventoryComponent;
class UTDEnhanceWindowWidget;
class UTDWindowBaseWidget;

UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDEnhanceServiceComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UTDEnhanceServiceComponent();

	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

	/** 서버의 정상 대화 종료 처리에서만 호출합니다. */
	void StartForNPC(ATDNPCBase* NPC);

	/** 새 대화 시작, 거리 초과 등에 의해 서버에서 종료합니다. */
	void EndService();

	UFUNCTION(Server, Reliable)
	void ServerCloseService(int32 SessionId);

	UFUNCTION(Server, Reliable)
	void ServerTryEnhance(
		int32 SessionId,
		int64 ExpectedRevision,
		int32 SlotIndex,
		int32 RequestId);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "TD|Enhance")
	TSubclassOf<UTDEnhanceWindowWidget> EnhanceWindowClass;

private:
	APlayerController* GetController() const;
	UTDInventoryComponent* GetInventory() const;

	bool IsServerContextValid() const;
	void TickServer();

	FTDEnhanceWindowView MakeView(
		const FString& Message,
		int32 ReplyRequestId,
		bool bResetSelection);

	void SendView(
		const FString& Message = FString(),
		int32 ReplyRequestId = 0,
		bool bResetSelection = true);

	UFUNCTION(Client, Reliable)
	void ClientOpenService(const FTDEnhanceWindowView& View);

	UFUNCTION(Client, Reliable)
	void ClientUpdateService(const FTDEnhanceWindowView& View);

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
	FTDEnhanceWindowView LocalView;

	UPROPERTY(Transient)
	TObjectPtr<UTDEnhanceWindowWidget> LocalWindow;

	bool bCursorBeforeWindow = false;
};