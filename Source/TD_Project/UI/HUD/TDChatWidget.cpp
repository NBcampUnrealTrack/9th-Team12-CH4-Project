#include "UI/HUD/TDChatWidget.h"
#include "UI/ViewModel/TDChatSubsystem.h"
#include "UI/Core/TDUIManagerSubsystem.h"
#include "UI/Common/Typography/TDTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/InputComponent.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"

void UTDChatWidget::NativeConstruct()
{
 Super::NativeConstruct();
 if (!MessageList || !MessageInput || !SendChannel) return;
 for (UTDTextBlock* Template : {MessageStyleAll.Get(), MessageStyleParty.Get(), MessageStyleWhisper.Get(),
  MessageStyleSystem.Get(), MessageStyleLoot.Get(), MessageStyleFailure.Get()})
 {
  if (Template)
  {
   if (UScrollBoxSlot* TemplateSlot = Cast<UScrollBoxSlot>(Template->Slot)) TemplatePadding.Add(Template, TemplateSlot->GetPadding());
  }
 }
 UnbindControls();
 if (TabAllButton) TabAllButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowAll);
 if (TabPartyButton) TabPartyButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowParty);
 if (TabWhisperButton) TabWhisperButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowWhisper);
 if (TabSystemButton) TabSystemButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowSystem);
 if (TabLootButton) TabLootButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowLoot);
 if (SendButton) SendButton->OnClicked.AddUniqueDynamic(this, &ThisClass::SendMessage);
 MessageInput->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleCommitted);
 SendChannel->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleChannelChanged);
 if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
 {
  Chat = LocalPlayer->GetSubsystem<UTDChatSubsystem>();
  if (Chat.IsValid())
  {
   Chat->RefreshSource();
   Chat->OnHistoryChanged.RemoveAll(this);
   Chat->OnHistoryChanged.AddUObject(this, &ThisClass::RefreshHistory);
  }
 }
 SendChannel->SetSelectedIndex(0);
 HandleChannelChanged(SendChannel->GetSelectedOption(), ESelectInfo::Direct);
 SetFilter(ETDChatChannel::All);
 if (APlayerController* Controller = GetOwningPlayer(); Controller && Controller->IsLocalController())
 {
  if (!ChatKeyInput)
  {
   ChatKeyInput = NewObject<UInputComponent>(this, NAME_None, RF_Transient);
   ChatKeyInput->BindKey(EKeys::Enter, IE_Pressed, this, &ThisClass::BeginChatInput);
   ChatKeyInput->bBlockInput = false;
  }
  KeyController = Controller;
  Controller->PopInputComponent(ChatKeyInput);
  Controller->PushInputComponent(ChatKeyInput);
 }
}

void UTDChatWidget::UnbindControls()
{
 if (TabAllButton) TabAllButton->OnClicked.RemoveDynamic(this, &ThisClass::ShowAll);
 if (TabPartyButton) TabPartyButton->OnClicked.RemoveDynamic(this, &ThisClass::ShowParty);
 if (TabWhisperButton) TabWhisperButton->OnClicked.RemoveDynamic(this, &ThisClass::ShowWhisper);
 if (TabSystemButton) TabSystemButton->OnClicked.RemoveDynamic(this, &ThisClass::ShowSystem);
 if (TabLootButton) TabLootButton->OnClicked.RemoveDynamic(this, &ThisClass::ShowLoot);
 if (SendButton) SendButton->OnClicked.RemoveDynamic(this, &ThisClass::SendMessage);
 if (MessageInput) MessageInput->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandleCommitted);
 if (SendChannel) SendChannel->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleChannelChanged);
}

void UTDChatWidget::NativeDestruct()
{
 FinishChatInput();
 if (Chat.IsValid()) Chat->OnHistoryChanged.RemoveAll(this);
 Chat.Reset();
 if (KeyController.IsValid() && ChatKeyInput) KeyController->PopInputComponent(ChatKeyInput);
 KeyController.Reset();
 UnbindControls();
 Super::NativeDestruct();
}

void UTDChatWidget::BeginChatInput()
{
 if (!IsVisible() || !GetIsEnabled() || !MessageInput) return;
 if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
 {
  if (UTDUIManagerSubsystem* Manager = LocalPlayer->GetSubsystem<UTDUIManagerSubsystem>())
  {
   if (Manager->BeginChatInput(this))
   {
    bEditing = true;
    FocusMessageInput();
   }
  }
 }
}

void UTDChatWidget::FocusMessageInput()
{
 if (MessageInput) MessageInput->SetUserFocus(GetOwningPlayer());
}

void UTDChatWidget::FinishChatInput()
{
 bEditing = false;
 if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
 {
  if (UTDUIManagerSubsystem* Manager = LocalPlayer->GetSubsystem<UTDUIManagerSubsystem>()) Manager->EndChatInput(this);
 }
}

void UTDChatWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
 Super::NativeTick(MyGeometry, InDeltaTime);
 // 탭/스크롤 포커스는 채팅 입력으로 취급하지 않는다. 클릭이 끝난 뒤 입력칸만 확인한다.
 if (bEditing || !FSlateApplication::IsInitialized()
  || FSlateApplication::Get().GetPressedMouseButtons().Num() > 0) return;
 APlayerController* Controller = GetOwningPlayer();
 if (!Controller) return;
 const auto IsTextInputFocused = [Controller](const UEditableTextBox* Input)
 {
  return Input && (Input->HasUserFocus(Controller) || Input->HasUserFocusedDescendants(Controller));
 };
 if (IsTextInputFocused(MessageInput) || IsTextInputFocused(WhisperTargetInput))
 {
  if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
   if (UTDUIManagerSubsystem* Manager = LocalPlayer->GetSubsystem<UTDUIManagerSubsystem>())
    bEditing = Manager->BeginChatInput(this);
 }
}

void UTDChatWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
 Super::NativeOnRemovedFromFocusPath(InFocusEvent);
 if (bEditing) FinishChatInput();
}

FReply UTDChatWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
 if (Event.GetKey() == EKeys::Escape && bEditing)
 {
  FinishChatInput();
  return FReply::Handled();
 }
 return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UTDChatWidget::HandleCommitted(const FText& Text, ETextCommit::Type Method)
{
 if (Method == ETextCommit::OnEnter)
 {
  if (Text.ToString().TrimStartAndEnd().IsEmpty()) FinishChatInput();
  else SendMessage();
 }
 else if (Method == ETextCommit::OnCleared) FinishChatInput();
}

void UTDChatWidget::HandleChannelChanged(FString Item, ESelectInfo::Type SelectionType)
{
 if (WhisperTargetRow && SendChannel)
  WhisperTargetRow->SetVisibility(SendChannel->GetSelectedIndex() == 2 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UTDChatWidget::SendMessage()
{
 if (!Chat.IsValid() || !MessageInput || !SendChannel) return;
 const int32 Index = SendChannel->GetSelectedIndex();
 const ETDChatChannel Channel = Index == 1 ? ETDChatChannel::Party : Index == 2 ? ETDChatChannel::Whisper : ETDChatChannel::All;
 if (Chat->Send(Channel, MessageInput->GetText().ToString(), WhisperTargetInput ? WhisperTargetInput->GetText().ToString() : FString()))
  MessageInput->SetText(FText::GetEmpty());
 if (bEditing) FocusMessageInput();
}

void UTDChatWidget::ShowAll() { SetFilter(ETDChatChannel::All); }
void UTDChatWidget::ShowParty() { SetFilter(ETDChatChannel::Party); }
void UTDChatWidget::ShowWhisper() { SetFilter(ETDChatChannel::Whisper); }
void UTDChatWidget::ShowSystem() { SetFilter(ETDChatChannel::System); }
void UTDChatWidget::ShowLoot() { SetFilter(ETDChatChannel::Loot); }

void UTDChatWidget::SetFilter(ETDChatChannel Channel)
{
 FilterChannel = Channel;
 UWidgetSwitcher* States[] = {TabAllState, TabPartyState, TabWhisperState, TabSystemState, TabLootState};
 const ETDChatChannel Channels[] = {ETDChatChannel::All, ETDChatChannel::Party, ETDChatChannel::Whisper, ETDChatChannel::System, ETDChatChannel::Loot};
 for (int32 Index = 0; Index < UE_ARRAY_COUNT(States); ++Index)
  if (States[Index]) States[Index]->SetActiveWidgetIndex(Channels[Index] == Channel ? 1 : 0);
 RenderMessages(true);
}

void UTDChatWidget::RefreshHistory() { RenderMessages(false); }

void UTDChatWidget::RenderMessages(bool bForceBottom)
{
 if (!MessageList) return;
 const float PreviousOffset = MessageList->GetScrollOffset();
 const bool bAtBottom = PreviousOffset >= MessageList->GetScrollOffsetOfEnd() - 8.f;
 MessageList->ClearChildren();
 if (!Chat.IsValid()) return;
 for (const FTDChatMessage& Entry : Chat->GetMessages())
 {
  // 시스템 메시지와 전송 오류도 전체/시스템 탭에서만 표시한다.
  if (FilterChannel != ETDChatChannel::All && Entry.Channel != FilterChannel) continue;
  UTDTextBlock* Template = MessageStyleAll;
  if (Entry.bSendFailure) Template = MessageStyleFailure;
  else switch (Entry.Channel)
  {
  case ETDChatChannel::Party: Template = MessageStyleParty; break;
  case ETDChatChannel::Whisper: Template = MessageStyleWhisper; break;
  case ETDChatChannel::System: Template = MessageStyleSystem; break;
  case ETDChatChannel::Loot: Template = MessageStyleLoot; break;
  default: break;
  }
  if (!Template) continue;
  // WBP 템플릿의 TDText 테마/색상/줄바꿈을 그대로 복제한다. C++에 폰트나 색상을 두지 않는다.
  UTDTextBlock* Row = DuplicateObject<UTDTextBlock>(Template, WidgetTree,
   MakeUniqueObjectName(WidgetTree, Template->GetClass(), TEXT("ChatMessage")));
  Row->Slot = nullptr;
  Row->SetVisibility(ESlateVisibility::HitTestInvisible);
  const FText ChannelText = UEnum::GetDisplayValueAsText(Entry.Channel);
  const FText Text = Entry.SenderName.IsEmpty()
   ? FText::Format(NSLOCTEXT("TDChat", "NoticeRow", "[{0}] {1}"), ChannelText, FText::FromString(Entry.Message))
   : FText::Format(NSLOCTEXT("TDChat", "PlayerRow", "[{0}] {1}: {2}"), ChannelText, FText::FromString(Entry.SenderName), FText::FromString(Entry.Message));
  Row->SetText(Text);
  if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(MessageList->AddChild(Row)))
   RowSlot->SetPadding(TemplatePadding.FindRef(Template));
 }
 if (bForceBottom || bAtBottom) MessageList->ScrollToEnd();
 else MessageList->SetScrollOffset(PreviousOffset);
}
