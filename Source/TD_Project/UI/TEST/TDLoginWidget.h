#pragma once
#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "TDLoginWidget.generated.h"

class UEditableTextBox;
class UVerticalBox;
class UTextBlock;
class UButton;

/** 화면 배치와 문구는 WBP_Login, 요청 처리와 데이터 연결은 C++. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDLoginWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()
protected:
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Text") FText WaitingMessage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Text") FText WrongControllerMessage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Text") FText EmptySlotFormat;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Text") FText CharacterSlotFormat;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UEditableTextBox> LoginInput;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UEditableTextBox> PasswordInput;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UVerticalBox> LoginPanel;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UVerticalBox> CharacterPanel;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Status;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> LoginButton;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> SlotButton0;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> SlotLabel0;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> SlotButton1;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> SlotLabel1;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> SlotButton2;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> SlotLabel2;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> SlotButton3;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> SlotLabel3;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> SlotButton4;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> SlotLabel4;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> SlotButton5;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> SlotLabel5;
private:
    UFUNCTION() void Login();
    UFUNCTION() void Select0();
    UFUNCTION() void Select1();
    UFUNCTION() void Select2();
    UFUNCTION() void Select3();
    UFUNCTION() void Select4();
    UFUNCTION() void Select5();
    void Select(int32 SlotIndex);
    void Refresh();
    UPROPERTY() TArray<TObjectPtr<UButton>> SlotButtons;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> SlotLabels;
    float RefreshElapsed = 0.f;
};
