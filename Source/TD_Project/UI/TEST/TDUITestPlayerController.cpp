#include "TDUITestPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"

ATDUITestPlayerController::ATDUITestPlayerController()
{
	static ConstructorHelpers::FClassFinder<UUserWidget> StartupWidgetFinder(
		TEXT("/Game/UI/WBP_HUD"));

	if (StartupWidgetFinder.Succeeded())
	{
		StartupWidgetClass = StartupWidgetFinder.Class;
	}
}

void ATDUITestPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 서버의 원격 컨트롤러에는 화면이 없으므로 로컬 플레이어에서만 만든다.
	if (!IsLocalController() || !StartupWidgetClass)
	{
		return;
	}

	StartupWidget = CreateWidget<UUserWidget>(this, StartupWidgetClass);
	if (!StartupWidget)
	{
		return;
	}

	StartupWidget->AddToViewport();
	bShowMouseCursor = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}
