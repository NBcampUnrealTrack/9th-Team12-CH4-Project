#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UI/HUD/Nav/TDNavMenuTypes.h"
#include "TDUIManagerSubsystem.generated.h"

class UTDUIRootWidget;
class UTDWindowBaseWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FTDOnMenuWindowStateChanged,
	ETDNavMenuType, MenuType,
	bool, bIsOpen
);

/**
 * 로컬 플레이어 한 명의 화면 흐름과 WindowLayer 창을 관리한다.
 * 서버 권위 데이터는 소유하지 않고, 로컬 UI 생성/닫기만 담당한다.
 */
UCLASS()
class TD_PROJECT_API UTDUIManagerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	void RegisterRoot(UTDUIRootWidget* InRootWidget);
	void UnregisterRoot(UTDUIRootWidget* InRootWidget);

	UFUNCTION(BlueprintCallable, Category = "TD|UI")
	void RequestMenu(ETDNavMenuType MenuType);

	UFUNCTION(BlueprintPure, Category = "TD|UI")
	bool IsMenuOpen(ETDNavMenuType MenuType) const;

	UPROPERTY(BlueprintAssignable, Category = "TD|UI")
	FTDOnMenuWindowStateChanged OnMenuWindowStateChanged;

private:
	TWeakObjectPtr<UTDUIRootWidget> RootWidget;
	TMap<ETDNavMenuType, TWeakObjectPtr<UTDWindowBaseWidget>> OpenWindows;
	TMap<ETDNavMenuType, TWeakObjectPtr<UTDWindowBaseWidget>> PrecreatedWindows;
	TMap<ETDNavMenuType, FVector2D> SavedWindowPositions;

	void ToggleWindow(ETDNavMenuType MenuType);
	TSubclassOf<UTDWindowBaseWidget> ResolveWindowClass(ETDNavMenuType MenuType);
	void SaveWindowPosition(ETDNavMenuType MenuType, UTDWindowBaseWidget* Window);
	void RestoreWindowPosition(ETDNavMenuType MenuType, UTDWindowBaseWidget* Window) const;
	void ClearWindowRegistry();

	UFUNCTION()
	void HandleWindowClosed(UTDWindowBaseWidget* ClosedWindow);
};
