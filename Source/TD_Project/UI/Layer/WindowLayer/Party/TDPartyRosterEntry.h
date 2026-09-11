#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HUD/TDProgressBarAnimation.h"
#include "TDPartyRosterEntry.generated.h"
class ATDPlayerState;
class UTDPartyWindowWidget;
class UTDPlayerStatsViewModel;
class UTDTextBlock;
class UProgressBar;
class UTDProgressBarStyleDA;
class UImage;
class UButton;
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDPartyRosterEntry : public UUserWidget
{
 GENERATED_BODY()
public:
 void SetEntry(ATDPlayerState* InState, UTDPartyWindowWidget* InOwner, bool bMember);
 void RefreshEntry(bool bCanInvite, bool bPending);
 ATDPlayerState* GetEntryState() const { return State.Get(); }
protected:
  virtual void NativeConstruct() override;
 /** 데이터만 전달한다. 문구/이미지/표시 상태는 WBP에서 적용한다. */
 UFUNCTION(BlueprintImplementableEvent, Category="TD|UI|Party")
 void OnEntryPresentation(UTDPlayerStatsViewModel* Data, bool bMember, bool bLeader, bool bSelf, bool bInviteEnabled, bool bPending);
 UFUNCTION(BlueprintImplementableEvent, Category="TD|UI|Party")
 void OnHealthPresentation(float Fill, float Trail, float FlashOpacity, FLinearColor FillTint, FLinearColor TrailTint, FLinearColor FlashTint);
 virtual void NativeDestruct() override;
 virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|Progress Bar") TObjectPtr<UTDProgressBarStyleDA> HealthBarStyleData;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTDTextBlock> NameText;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTDTextBlock> DetailText;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTDTextBlock> HealthText;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTDTextBlock> InviteText;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UProgressBar> HealthBar;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> HealthLagBar;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> HealthRecoveryFlashBar;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UImage> Portrait;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UImage> ClassIcon;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> InviteButton;
private:
 TWeakObjectPtr<ATDPlayerState> State;
 TWeakObjectPtr<UTDPartyWindowWidget> OwnerWindow;
 UPROPERTY(Transient) TObjectPtr<UTDPlayerStatsViewModel> ViewModel;
 bool bPartyMember = false;
 FTDProgressBarAnimation HealthAnimation;
 void ApplyHealthVisuals();
 UFUNCTION() void HandleInvite();
};
