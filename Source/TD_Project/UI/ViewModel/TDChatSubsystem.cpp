#include "UI/ViewModel/TDChatSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Player/TDPlayerController.h"
#include "Player/TDPlayerState.h"
#include "Settings/TDChatSettings.h"

void UTDChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 RefreshSource();
 SourceTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::CheckSource), .25f);
}

void UTDChatSubsystem::Deinitialize()
{
 FTSTicker::GetCoreTicker().RemoveTicker(SourceTicker);
 SourceTicker.Reset();
 UnbindSource();
 Messages.Reset();
 OnHistoryChanged.Clear();
 Super::Deinitialize();
}

void UTDChatSubsystem::UnbindSource()
{
 if (ATDPlayerController* Controller = BoundController.Get())
 {
  Controller->OnChatReceived.RemoveDynamic(this, &ThisClass::HandleReceived);
  Controller->OnChatSendFailed.RemoveDynamic(this, &ThisClass::HandleSendFailed);
 }
 BoundController.Reset();
}

void UTDChatSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
 ATDPlayerController* Controller = IsValid(NewPlayerController) && NewPlayerController->IsLocalController()
  && NewPlayerController->GetLocalPlayer() == GetLocalPlayer() ? Cast<ATDPlayerController>(NewPlayerController) : nullptr;
 const ATDPlayerState* State = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;
 const bool bSelected = State && State->HasSelectedCharacter();
 const FString Name = bSelected ? State->GetPlayerName() : FString();
 const bool bControllerChanged = BoundController.Get() != Controller;
 if (bControllerChanged)
 {
  UnbindSource();
  BoundController = Controller;
  if (Controller)
  {
   Controller->OnChatReceived.AddUniqueDynamic(this, &ThisClass::HandleReceived);
   Controller->OnChatSendFailed.AddUniqueDynamic(this, &ThisClass::HandleSendFailed);
  }
 }
 if (bControllerChanged || bHadSelectedCharacter != bSelected || BoundPlayerName != Name)
 {
  bHadSelectedCharacter = bSelected;
  BoundPlayerName = Name;
  Messages.Reset();
  OnHistoryChanged.Broadcast();
 }
}

void UTDChatSubsystem::RefreshSource()
{
 PlayerControllerChanged(GetLocalPlayer()->GetPlayerController(GetWorld()));
}

bool UTDChatSubsystem::CheckSource(float DeltaTime)
{
 RefreshSource();
 return true;
}

void UTDChatSubsystem::Append(FTDChatMessage Entry)
{
 const int32 Limit = FMath::Clamp(UTDChatSettings::Get()->MaxHistoryMessages, 1, 1000);
 Messages.Add(MoveTemp(Entry));
 if (Messages.Num() > Limit) Messages.RemoveAt(0, Messages.Num() - Limit);
 OnHistoryChanged.Broadcast();
}

void UTDChatSubsystem::HandleReceived(ETDChatChannel Channel, FString SenderName, FString Message)
{
 RefreshSource();
 FTDChatMessage Entry;
 Entry.Channel = Channel;
 Entry.SenderName = SenderName;
 Entry.Message = Message;
 Append(MoveTemp(Entry));
}

void UTDChatSubsystem::HandleSendFailed(ETDChatSendResult Reason)
{
 FText Error;
 switch (Reason)
 {
 case ETDChatSendResult::Empty: Error = NSLOCTEXT("TDChat", "Empty", "메시지를 입력해주세요."); break;
 case ETDChatSendResult::TooLong:
  Error = FText::Format(NSLOCTEXT("TDChat", "TooLong", "메시지는 {0}자까지 보낼 수 있습니다."), FText::AsNumber(UTDChatSettings::Get()->MaxMessageLength)); break;
 case ETDChatSendResult::TooFast: Error = NSLOCTEXT("TDChat", "TooFast", "잠시 후 다시 보내주세요."); break;
 case ETDChatSendResult::NotInParty: Error = NSLOCTEXT("TDChat", "NoParty", "파티에 참가한 뒤 보낼 수 있습니다."); break;
 case ETDChatSendResult::TargetNotFound: Error = NSLOCTEXT("TDChat", "NoTarget", "귓속말 대상의 이름과 접속 상태를 확인해주세요."); break;
 case ETDChatSendResult::ChannelNotAllowed: Error = NSLOCTEXT("TDChat", "BadChannel", "이 채널에는 메시지를 보낼 수 없습니다."); break;
 default: return;
 }
 FTDChatMessage Entry;
 Entry.Channel = ETDChatChannel::System;
 Entry.Message = Error.ToString();
 Entry.bSendFailure = true;
 Append(MoveTemp(Entry));
}

bool UTDChatSubsystem::Send(ETDChatChannel Channel, const FString& Message, const FString& TargetName)
{
 RefreshSource();
 ATDPlayerController* Controller = BoundController.Get();
 if (!Controller) return false;
 const FString Trimmed = Message.TrimStartAndEnd();
 if (Trimmed.IsEmpty()) { HandleSendFailed(ETDChatSendResult::Empty); return false; }
 if (Trimmed.Len() > UTDChatSettings::Get()->MaxMessageLength) { HandleSendFailed(ETDChatSendResult::TooLong); return false; }
 if (Channel != ETDChatChannel::All && Channel != ETDChatChannel::Party && Channel != ETDChatChannel::Whisper)
 { HandleSendFailed(ETDChatSendResult::ChannelNotAllowed); return false; }
 const FString Target = TargetName.TrimStartAndEnd();
 if (Channel == ETDChatChannel::Whisper && Target.IsEmpty()) { HandleSendFailed(ETDChatSendResult::TargetNotFound); return false; }
 Controller->ServerSendChat(Channel, Trimmed, Target);
 return true;
}
