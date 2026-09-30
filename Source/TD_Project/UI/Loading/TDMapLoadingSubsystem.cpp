#include "UI/Loading/TDMapLoadingSubsystem.h"

#include "UI/Loading/TDMapLoadingSettings.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CoreDelegates.h"
#include "MoviePlayer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"

DEFINE_LOG_CATEGORY_STATIC(LogTDMapLoading, Log, All);

namespace
{
FLoadingScreenAttributes MakeLoadingScreenAttributes(const TSharedPtr<SWidget>& Widget)
{
    FLoadingScreenAttributes Attributes;
    Attributes.WidgetLoadingScreen = Widget;
    Attributes.MinimumLoadingScreenDisplayTime = -1.0f;
    Attributes.bAutoCompleteWhenLoadingCompletes = true;
    Attributes.bWaitForManualStop = false;
    Attributes.bMoviesAreSkippable = false;
    Attributes.bAllowEngineTick = false;
    return Attributes;
}
}

void UTDMapLoadingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    const auto* Settings = GetDefault<UTDMapLoadingSettings>();
    if (!Settings->bEnabled || IsRunningDedicatedServer() || IsRunningCommandlet()) return;

    // MoviePlayer is disabled in PIE. Use Standalone (-game) or a packaged client.
    if (!IsMoviePlayerEnabled() || !FSlateApplication::IsInitialized())
    {
        UE_LOG(LogTDMapLoading, Log,
            TEXT("Threaded loading screen disabled here. Use standalone (-game) or a packaged client, not PIE."));
        return;
    }

    const auto MoviePlayer = GetMoviePlayer();
    if (!MoviePlayer) return;

    const TSubclassOf<UUserWidget> ScreenClass = Settings->LoadingWidgetClass.LoadSynchronous();
    if (ScreenClass)
    {
        // The movie window and the real game window cannot share one Slate child.
        MovieLoadingScreen = CreateWidget<UUserWidget>(GetGameInstance(), ScreenClass);
        WindowLoadingScreen = CreateWidget<UUserWidget>(GetGameInstance(), ScreenClass);
    }

    if (MovieLoadingScreen && WindowLoadingScreen)
    {
        LoadingWidget = MovieLoadingScreen->TakeWidget();
        WindowLoadingWidget = WindowLoadingScreen->TakeWidget();
        // Circular Throbber animates in Slate without a UserWidget tick.
        LoadingWidget->SetCanTick(false);
        WindowLoadingWidget->SetCanTick(false);
        UE_LOG(LogTDMapLoading, Log, TEXT("Loading layout: %s"), *ScreenClass->GetPathName());
    }
    else
    {
        UE_LOG(LogTDMapLoading, Error, TEXT("Loading widget unavailable: %s; using native fallback."),
            *Settings->LoadingWidgetClass.ToString());
        LoadingWidget = FLoadingScreenAttributes::NewTestLoadingScreenWidget();
        WindowLoadingWidget = FLoadingScreenAttributes::NewTestLoadingScreenWidget();
    }

    WindowLoadingCover = SNew(SBorder)
        .Padding(0.0f)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(0.025f, 0.035f, 0.06f, 1.0f))
        [WindowLoadingWidget.ToSharedRef()];

    // The static engine-loading screen covers startup. Attach the WBP once the
    // pre-load manager has handed the window back to the game viewport.
    StartupHandoffHandle = FCoreDelegates::OnEndFrame.AddUObject(
        this, &ThisClass::AttachWindowOverlayAfterStartup);
    bWindowOverlayVisible = true;

    if (GEngine)
    {
        TravelFailureHandle = GEngine->OnTravelFailure().AddWeakLambda(this,
            [this](UWorld* World, ETravelFailure::Type, const FString&)
            {
                if (!World || World->GetGameInstance() == GetGameInstance()) HideWindowOverlay();
            });
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddWeakLambda(this,
            [this](UWorld* World, UNetDriver*, ENetworkFailure::Type, const FString&)
            {
                if (!World || World->GetGameInstance() == GetGameInstance()) HideWindowOverlay();
            });

        // Replace the engine's captured-world + three-dot streaming pause screen.
        PreviousBeginStreamingPauseDelegate = GEngine->BeginStreamingPauseDelegate;
        PreviousEndStreamingPauseDelegate = GEngine->EndStreamingPauseDelegate;
        BeginStreamingPauseDelegate.BindUObject(this, &ThisClass::BeginStreamingPause);
        EndStreamingPauseDelegate.BindUObject(this, &ThisClass::EndStreamingPause);
        GEngine->RegisterBeginStreamingPauseRenderingDelegate(&BeginStreamingPauseDelegate);
        GEngine->RegisterEndStreamingPauseRenderingDelegate(&EndStreamingPauseDelegate);
    }

    PrepareHandle = MoviePlayer->OnPrepareLoadingScreen().AddUObject(
        this, &ThisClass::PrepareLoadingScreen);
    UE_LOG(LogTDMapLoading, Log, TEXT("Map and streaming-pause loading screens registered."));
}

void UTDMapLoadingSubsystem::PrepareLoadingScreen()
{
    const auto MoviePlayer = GetMoviePlayer();
    if (!MoviePlayer || !LoadingWidget.IsValid()) return;

    bWindowOverlayVisible = true;
    ShowWindowOverlay();

    MoviePlayer->SetupLoadingScreen(MakeLoadingScreenAttributes(LoadingWidget));
    bPreparedScreen = true;
}

void UTDMapLoadingSubsystem::ShowWindowOverlay()
{
    if (!bWindowOverlayVisible || !WindowLoadingCover.IsValid()) return;

    UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    if (!Window.IsValid() || !Window->HasOverlay() || CoveredWindow.Pin() == Window) return;

    if (const TSharedPtr<SWindow> Previous = CoveredWindow.Pin())
    {
        Previous->RemoveOverlaySlot(WindowLoadingCover.ToSharedRef());
    }

    CoveredWindow = Window;
    Window->AddOverlaySlot(10000)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        [WindowLoadingCover.ToSharedRef()];

    UE_LOG(LogTDMapLoading, Log, TEXT("Window loading cover attached."));
}

void UTDMapLoadingSubsystem::AttachWindowOverlayAfterStartup()
{
    if (!bWindowOverlayVisible)
    {
        FCoreDelegates::OnEndFrame.Remove(StartupHandoffHandle);
        StartupHandoffHandle.Reset();
        return;
    }

    // A local map load may have attached the cover before the window handoff.
    // Discard that stale slot record and bind to the final game window.
    DetachWindowOverlay();
    ShowWindowOverlay();

    // If the window is not ready yet, keep the delegate and retry next frame.
    if (CoveredWindow.IsValid())
    {
        FCoreDelegates::OnEndFrame.Remove(StartupHandoffHandle);
        StartupHandoffHandle.Reset();
        UE_LOG(LogTDMapLoading, Log, TEXT("Startup-to-WBP loading-screen handoff completed."));
    }
}

void UTDMapLoadingSubsystem::HideLoadingScreen()
{
    if (!bWindowOverlayVisible) return;

    UE_LOG(LogTDMapLoading, Log, TEXT("Usable UI activated; removing the loading cover."));
    HideWindowOverlay();
}

void UTDMapLoadingSubsystem::HideWindowOverlay()
{
    bWindowOverlayVisible = false;
    DetachWindowOverlay();
}

void UTDMapLoadingSubsystem::DetachWindowOverlay()
{
    if (const TSharedPtr<SWindow> Window = CoveredWindow.Pin())
    {
        if (WindowLoadingCover.IsValid())
        {
            Window->RemoveOverlaySlot(WindowLoadingCover.ToSharedRef());
        }
    }
    CoveredWindow.Reset();
}

void UTDMapLoadingSubsystem::BeginStreamingPause(FViewport*)
{
    const auto MoviePlayer = GetMoviePlayer();
    if (!MoviePlayer || !MoviePlayer->IsInitialized() || MoviePlayer->IsMovieCurrentlyPlaying()
        || !LoadingWidget.IsValid()) return;

    MoviePlayer->SetupLoadingScreen(MakeLoadingScreenAttributes(LoadingWidget));
    bStreamingPauseMovieStarted = MoviePlayer->PlayMovie();
}

void UTDMapLoadingSubsystem::EndStreamingPause()
{
    if (!bStreamingPauseMovieStarted) return;

    GetMoviePlayer()->WaitForMovieToFinish();
    bStreamingPauseMovieStarted = false;
}

void UTDMapLoadingSubsystem::Deinitialize()
{
    if (bStreamingPauseMovieStarted)
    {
        EndStreamingPause();
    }

    if (StartupHandoffHandle.IsValid())
    {
        FCoreDelegates::OnEndFrame.Remove(StartupHandoffHandle);
        StartupHandoffHandle.Reset();
    }

    if (GEngine)
    {
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);

        if (GEngine->BeginStreamingPauseDelegate == &BeginStreamingPauseDelegate)
        {
            GEngine->RegisterBeginStreamingPauseRenderingDelegate(PreviousBeginStreamingPauseDelegate);
        }
        if (GEngine->EndStreamingPauseDelegate == &EndStreamingPauseDelegate)
        {
            GEngine->RegisterEndStreamingPauseRenderingDelegate(PreviousEndStreamingPauseDelegate);
        }
    }

    TravelFailureHandle.Reset();
    NetworkFailureHandle.Reset();
    BeginStreamingPauseDelegate.Unbind();
    EndStreamingPauseDelegate.Unbind();
    PreviousBeginStreamingPauseDelegate = nullptr;
    PreviousEndStreamingPauseDelegate = nullptr;
    HideWindowOverlay();

    if (PrepareHandle.IsValid())
    {
        if (const auto MoviePlayer = GetMoviePlayer())
        {
            MoviePlayer->OnPrepareLoadingScreen().Remove(PrepareHandle);
            if (bPreparedScreen)
            {
                if (MoviePlayer->IsMovieCurrentlyPlaying())
                {
                    MoviePlayer->StopMovie();
                    MoviePlayer->WaitForMovieToFinish(false);
                }
                MoviePlayer->SetupLoadingScreen(FLoadingScreenAttributes());
            }
        }
        PrepareHandle.Reset();
    }

    WindowLoadingCover.Reset();
    WindowLoadingWidget.Reset();
    LoadingWidget.Reset();
    WindowLoadingScreen = nullptr;
    MovieLoadingScreen = nullptr;
    Super::Deinitialize();
}
