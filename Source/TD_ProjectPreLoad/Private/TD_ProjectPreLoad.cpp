#include "Modules/ModuleManager.h"

#include "Brushes/SlateDynamicImageBrush.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "PreLoadScreenBase.h"
#include "PreLoadScreenManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScaleBox.h"

DEFINE_LOG_CATEGORY_STATIC(LogTDProjectPreLoad, Log, All);

namespace
{
constexpr TCHAR StartupImageRelativePath[] = TEXT("Loading/TD_Startup.png");

TSharedRef<SWidget> CreateStartupImageWidget(const FSlateBrush* BackgroundBrush)
{
    return SNew(SScaleBox)
        .Stretch(EStretch::ScaleToFill)
        .StretchDirection(EStretchDirection::Both)
        .Clipping(EWidgetClipping::ClipToBounds)
        [
            SNew(SImage)
            .Image(BackgroundBrush)
        ];
}

class FTDProjectPreLoadScreen final : public FPreLoadScreenBase
{
public:
    FTDProjectPreLoadScreen(
        const EPreLoadScreenTypes InScreenType,
        const TSharedRef<FSlateDynamicImageBrush>& InBackgroundBrush)
        : ScreenType(InScreenType)
        , BackgroundBrush(InBackgroundBrush)
    {
    }

    virtual void Init() override
    {
        LoadingWidget = CreateStartupImageWidget(&BackgroundBrush.Get());
    }

    virtual void Tick(float) override
    {
        if (ScreenType == EPreLoadScreenTypes::EarlyStartupScreen)
        {
            bEarlyStartupFrameTicked = true;
        }
    }

    virtual bool IsDone() const override
    {
        if (ScreenType == EPreLoadScreenTypes::EarlyStartupScreen)
        {
            return bEarlyStartupFrameTicked;
        }

        return FPreLoadScreenBase::IsDone();
    }

    virtual EPreLoadScreenTypes GetPreLoadScreenType() const override
    {
        return ScreenType;
    }

    virtual TSharedPtr<SWidget> GetWidget() override
    {
        return LoadingWidget;
    }

    virtual const TSharedPtr<const SWidget> GetWidget() const override
    {
        return LoadingWidget;
    }

private:
    EPreLoadScreenTypes ScreenType;
    TSharedRef<FSlateDynamicImageBrush> BackgroundBrush;
    TSharedPtr<SWidget> LoadingWidget;
    bool bEarlyStartupFrameTicked = false;
};
}

class FTDProjectPreLoadModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
#if !UE_SERVER
        if (GIsEditor || !FApp::CanEverRender() || IsRunningDedicatedServer() || IsRunningCommandlet())
        {
            return;
        }

        const FString ImagePath = FPaths::Combine(FPaths::ProjectContentDir(), StartupImageRelativePath);
        if (!FPaths::FileExists(ImagePath))
        {
            UE_LOG(LogTDProjectPreLoad, Error, TEXT("Startup image not found: %s"), *ImagePath);
            return;
        }

        FPreLoadScreenManager* Manager = FPreLoadScreenManager::Get();
        if (!Manager)
        {
            UE_LOG(LogTDProjectPreLoad, Error, TEXT("Pre-load screen manager is unavailable."));
            return;
        }

        const TSharedRef<FSlateDynamicImageBrush> BackgroundBrush = MakeShared<FSlateDynamicImageBrush>(
            FName(*ImagePath), FVector2D(1672.0, 941.0));

        const TSharedPtr<FTDProjectPreLoadScreen> EarlyStartupScreen = MakeShared<FTDProjectPreLoadScreen>(
            EPreLoadScreenTypes::EarlyStartupScreen, BackgroundBrush);
        const TSharedPtr<FTDProjectPreLoadScreen> EngineLoadingScreen = MakeShared<FTDProjectPreLoadScreen>(
            EPreLoadScreenTypes::EngineLoadingScreen, BackgroundBrush);

        EarlyStartupScreen->Init();
        EngineLoadingScreen->Init();
        Manager->RegisterPreLoadScreen(EarlyStartupScreen);
        Manager->RegisterPreLoadScreen(EngineLoadingScreen);

        UE_LOG(LogTDProjectPreLoad, Log,
            TEXT("Early-startup and engine-loading screens registered with %s."), *ImagePath);
#endif
    }
};

IMPLEMENT_MODULE(FTDProjectPreLoadModule, TD_ProjectPreLoad)
