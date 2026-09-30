#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "TDLoginWidget.generated.h"

class UTDAccountScreenPresenter;

/** 화면 구성은 WBP_Login, 계정 요청과 상태 갱신은 Presenter가 담당한다. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDLoginWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

protected:
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UTDAccountScreenPresenter> AccountPresenter;

    float RefreshElapsed = 0.f;
};
