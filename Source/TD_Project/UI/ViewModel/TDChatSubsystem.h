#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Chat/TDChatTypes.h"
#include "TDChatSubsystem.generated.h"

class ATDPlayerController;

USTRUCT(BlueprintType)
struct FTDChatMessage
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) ETDChatChannel Channel = ETDChatChannel::All;
 UPROPERTY(BlueprintReadOnly) FString SenderName;
 UPROPERTY(BlueprintReadOnly) FString Message;
 UPROPERTY(BlueprintReadOnly) bool bSendFailure = false;
};

DECLARE_MULTICAST_DELEGATE(FTDOnChatHistoryChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FTDOnChatMessageReceived, const FTDChatMessage&);

/** 로컬 플레이어의 채팅 수신/오류 구독과 제한된 세션 이력. 화면 스타일은 소유하지 않는다. */
UCLASS()
class TD_PROJECT_API UTDChatSubsystem : public ULocalPlayerSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
 const TArray<FTDChatMessage>& GetMessages() const { return Messages; }
 FTDOnChatHistoryChanged OnHistoryChanged;
 /** 서버에서 수신한 새 메시지만 전달한다. 이력 갱신/전송 오류는 포함하지 않는다. */
 FTDOnChatMessageReceived OnMessageReceived;

 /** 서버로 요청을 보냈는지 반환한다. 최종 성공/실패는 서버 응답으로 반영한다. */
 bool Send(ETDChatChannel Channel, const FString& Message, const FString& TargetName);
 void RefreshSource();

private:
 UPROPERTY(Transient) TArray<FTDChatMessage> Messages;
 TWeakObjectPtr<ATDPlayerController> BoundController;
 FTSTicker::FDelegateHandle SourceTicker;
 bool bHadSelectedCharacter = false;
 FString BoundPlayerName;
 bool CheckSource(float DeltaTime);
 void UnbindSource();
 void Append(FTDChatMessage Entry);
 UFUNCTION() void HandleReceived(ETDChatChannel Channel, FString SenderName, FString Message);
 UFUNCTION() void HandleSendFailed(ETDChatSendResult Reason);
};
