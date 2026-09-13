#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Chat/TDChatTypes.h"
#include "Components/EditableTextBox.h"
#include "TDChatWidget.generated.h"

class UButton;
class UComboBoxString;
class UScrollBox;
class UWidgetSwitcher;
class UTDTextBlock;
class UTDChatSubsystem;
class UInputComponent;

/** 동작만 C++로 연결한다. 배치/탭 상태/메시지 색상/폰트는 WBP_Chat의 위젯과 템플릿에서 편집한다. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDChatWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void BeginChatInput();
 void FinishChatInput();
 void FocusMessageInput();
 void NotifyInputClosed() { bEditing = false; }
 bool IsEditing() const { return bEditing; }

protected:
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
 virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
 virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

 UPROPERTY(meta=(BindWidget)) TObjectPtr<UScrollBox> MessageList;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> MessageInput;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> WhisperTargetInput;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidget> WhisperTargetRow;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> SendChannel;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> SendButton;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> TabAllButton;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> TabPartyButton;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> TabWhisperButton;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> TabSystemButton;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> TabLootButton;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> TabAllState;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> TabPartyState;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> TabWhisperState;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> TabSystemState;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> TabLootState;

 /** 디자이너의 예시 메시지 행을 실행 중 메시지 스타일 템플릿으로 사용한다. */
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UTDTextBlock> MessageStyleAll;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UTDTextBlock> MessageStyleParty;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UTDTextBlock> MessageStyleWhisper;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UTDTextBlock> MessageStyleSystem;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UTDTextBlock> MessageStyleLoot;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UTDTextBlock> MessageStyleFailure;

private:
 TWeakObjectPtr<UTDChatSubsystem> Chat;
 UPROPERTY(Transient) TObjectPtr<UInputComponent> ChatKeyInput;
 TWeakObjectPtr<APlayerController> KeyController;
 TMap<const UTDTextBlock*, FMargin> TemplatePadding;
 ETDChatChannel FilterChannel = ETDChatChannel::All;
 bool bEditing = false;
 void SetFilter(ETDChatChannel Channel);
 void RefreshHistory();
 void RenderMessages(bool bForceBottom);
 void UnbindControls();
 UFUNCTION() void ShowAll();
 UFUNCTION() void ShowParty();
 UFUNCTION() void ShowWhisper();
 UFUNCTION() void ShowSystem();
 UFUNCTION() void ShowLoot();
 UFUNCTION() void SendMessage();
 UFUNCTION() void HandleCommitted(const FText& Text, ETextCommit::Type Method);
 UFUNCTION() void HandleChannelChanged(FString Item, ESelectInfo::Type SelectionType);
};
