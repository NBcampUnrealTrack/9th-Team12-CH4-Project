#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FieldNotificationId.h"
#include "UI/HUD/TDPlayerVitalAnimation.h"
#include "TDPlayerStatusWidget.generated.h"

class UTDPlayerStatsViewModel;
class UTextBlock;
class UProgressBar;
class UImage;
class UTexture2D;

/** 디자이너의 배치 + 공유 뷰모델의 필요한 필드만 구독하는 플레이어 HUD. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDPlayerStatusWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable, Category="TD|UI|Player Status")
 void SetViewModel(UTDPlayerStatsViewModel* InViewModel);

 UFUNCTION(BlueprintPure, Category="TD|UI|Player Status")
 UTDPlayerStatsViewModel* GetViewModel() const { return ViewModel; }

 /** 전용 초상화는 UVScale=(1,1), UVOffset=(0,0)으로 전달한다. */
 UFUNCTION(BlueprintCallable, Category="TD|UI|Player Status")
 void SetMainPortrait(UTexture2D* Texture, FVector2D UVScale, FVector2D UVOffset);

protected:
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

 /** 연출은 이 HUD 안에서만 처리한다. 공유 뷰모델의 실제 값은 지연시키지 않는다. */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Vital Effects")
 bool bAnimateVitals = true;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Vital Effects", meta=(ClampMin="0", Units="s"))
 float HealthDamageDelaySeconds = 0.18f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Vital Effects", meta=(ClampMin="0", Units="s"))
 float HealthDamageFadeSeconds = 0.4f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Vital Effects", meta=(ClampMin="0", Units="s"))
 float ManaChangeSeconds = 0.2f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Vital Effects", meta=(ClampMin="0", Units="s"))
 float RecoverySeconds = 0.25f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Vital Effects", meta=(ClampMin="0", Units="s"))
 float RecoveryFlashSeconds = 0.35f;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Vital Effects", meta=(ClampMin="0", ClampMax="1"))
 float RecoveryFlashOpacity = 0.32f;

 UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="TD|UI")
 TObjectPtr<UTextBlock> LevelNameText;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="TD|UI")
 TObjectPtr<UTextBlock> HealthValueText;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="TD|UI")
 TObjectPtr<UTextBlock> ManaValueText;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="TD|UI")
 TObjectPtr<UProgressBar> HealthBar;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="TD|UI")
 TObjectPtr<UProgressBar> ManaBar;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="TD|UI")
 TObjectPtr<UProgressBar> HealthLagBar;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="TD|UI")
 TObjectPtr<UProgressBar> HealthRecoveryFlashBar;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="TD|UI")
 TObjectPtr<UProgressBar> ManaRecoveryFlashBar;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="TD|UI")
 TObjectPtr<UImage> MainPortrait;
 UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="TD|UI")
 TObjectPtr<UImage> SubPortraitPlaceholder;

private:
 UPROPERTY(Transient)
 TObjectPtr<UTDPlayerStatsViewModel> ViewModel;
 FTDPlayerVitalAnimation HealthAnimation;
 FTDPlayerVitalAnimation ManaAnimation;
 bool bVitalsPending = false;
 void RefreshVitalTargets(bool bAnimate);
 void ApplyVitalVisuals();
 void UnbindViewModel();
 void RefreshField(UE::FieldNotification::FFieldId Field);
 void OnFieldChanged(UObject* Object, UE::FieldNotification::FFieldId Field);
};
