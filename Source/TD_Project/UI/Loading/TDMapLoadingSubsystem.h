#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TDMapLoadingSubsystem.generated.h"

class SWidget;
class UUserWidget;
class SWindow;

UCLASS()
class TD_PROJECT_API UTDMapLoadingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void HideLoadingScreen();

private:
	void PrepareLoadingScreen();
	void ShowWindowOverlay();
	void AttachWindowOverlayAfterStartup();
	void HideWindowOverlay();
	void DetachWindowOverlay();
	void BeginStreamingPause(FViewport* GameViewport);
	void EndStreamingPause();

	UPROPERTY(Transient)
		TObjectPtr<UUserWidget> MovieLoadingScreen;

	UPROPERTY(Transient)
		TObjectPtr<UUserWidget> WindowLoadingScreen;

	TSharedPtr<SWidget> LoadingWidget;
	TSharedPtr<SWidget> WindowLoadingWidget;
	TSharedPtr<SWidget> WindowLoadingCover;
	TWeakPtr<SWindow> CoveredWindow;

	FDelegateHandle PrepareHandle;
	FDelegateHandle StartupHandoffHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle NetworkFailureHandle;

	FBeginStreamingPauseDelegate BeginStreamingPauseDelegate;
	FEndStreamingPauseDelegate EndStreamingPauseDelegate;
	FBeginStreamingPauseDelegate* PreviousBeginStreamingPauseDelegate = nullptr;
	FEndStreamingPauseDelegate* PreviousEndStreamingPauseDelegate = nullptr;

	bool bWindowOverlayVisible = false;
	bool bPreparedScreen = false;
	bool bStreamingPauseMovieStarted = false;
};
