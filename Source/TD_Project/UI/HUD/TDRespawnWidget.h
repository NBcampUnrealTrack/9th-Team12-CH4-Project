#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "TDRespawnWidget.generated.h"

class UButton;
class UTextBlock;
class ATDPlayerController;

/** Presentation only. The existing controller RPC and GameMode own respawning. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDRespawnWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="TD|Respawn")
    void RequestRespawn();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
    TObjectPtr<UButton> RespawnButton;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> CountdownText;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|Respawn")
    FText CountdownFormat = NSLOCTEXT("TDRespawn", "Countdown", "{Seconds}초 후 자동으로 부활합니다");

private:
    UFUNCTION()
    void HandleRespawnRequestResult(bool bSucceeded);
    void UnbindRespawnRequestResult();
    TWeakObjectPtr<ATDPlayerController> BoundRespawnController;
    void RefreshCountdown();
    bool bRequestSent = false;
};
