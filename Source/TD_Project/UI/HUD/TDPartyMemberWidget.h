#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FieldNotificationId.h"
#include "TDPartyMemberWidget.generated.h"

class ATDPlayerState;
class UTDPlayerStatsViewModel;
class UImage;
class UTextBlock;
class UProgressBar;

/** 파티원 한 줄. 배치/장식은 WBP, 표시 데이터만 C++에서 연결한다. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDPartyMemberWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetMember(ATDPlayerState* InMember);
    void RefreshMember();
    UFUNCTION(BlueprintPure, Category="TD|UI|Party")
    ATDPlayerState* GetMember() const { return Member.Get(); }
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Portrait;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> ClassIcon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> LeaderIcon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> LevelNameText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HealthValueText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ClassFallbackText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> HealthBar;
private:
    TWeakObjectPtr<ATDPlayerState> Member;
    UPROPERTY(Transient) TObjectPtr<UTDPlayerStatsViewModel> ViewModel;
    void ConnectViewModel();
    void ApplyDisplay();
    void HandleFieldChanged(UObject* Object, UE::FieldNotification::FFieldId Field);
};
