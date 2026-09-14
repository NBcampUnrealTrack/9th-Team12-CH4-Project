#include "UI/TEST/TDLoginWidget.h"
#include "UI/Account/TDAccountScreenPresenter.h"
#include "Input/CommonUIInputTypes.h"

void UTDLoginWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (AccountPresenter) AccountPresenter->Shutdown();
    RefreshElapsed = 0.f;
    AccountPresenter = NewObject<UTDAccountScreenPresenter>(this);
    if (!AccountPresenter->Initialize(this))
    {
        AccountPresenter->Shutdown();
        AccountPresenter = nullptr;
        UE_LOG(LogTemp, Error, TEXT("WBP_Login account pages could not be initialized."));
    }
}

void UTDLoginWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    RefreshElapsed += DeltaTime;
    if (RefreshElapsed >= .1f)
    {
        RefreshElapsed = 0.f;
        if (AccountPresenter) AccountPresenter->Refresh();
    }
}

TOptional<FUIInputConfig> UTDLoginWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UTDLoginWidget::NativeDestruct()
{
    if (AccountPresenter) AccountPresenter->Shutdown();
    AccountPresenter = nullptr;
    Super::NativeDestruct();
}
