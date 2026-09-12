#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UI/HUD/Nav/TDNavMenuTypes.h"
#include "TDUIManagerSubsystem.generated.h"

class ATDPlayerState;
class UTDLoginWidget;
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

	/** Root의 ScreenStack에서 로그인/선택 화면을 관리한다. */
	void StartAccountFlow(TSubclassOf<UTDLoginWidget> WidgetClass);
	UFUNCTION(BlueprintCallable, Category="TD|UI|Account")
		void RequestCharacterSelection();
	UFUNCTION(BlueprintCallable, Category="TD|UI|Account")
		void RequestLogout();

	void RegisterRoot(UTDUIRootWidget* InRootWidget);
	void UnregisterRoot(UTDUIRootWidget* InRootWidget);

	UFUNCTION(BlueprintCallable, Category = "TD|UI")
		void RequestMenu(ETDNavMenuType MenuType);
	void ShowPartyInvitation(ATDPlayerState* Inviter);

	UFUNCTION(BlueprintPure, Category = "TD|UI")
		bool IsMenuOpen(ETDNavMenuType MenuType) const;

	UPROPERTY(BlueprintAssignable, Category = "TD|UI")
		FTDOnMenuWindowStateChanged OnMenuWindowStateChanged;

	/** WindowLayer 안에서 해당 창을 가장 앞으로 배치한다. */
	void BringWindowToFront(UTDWindowBaseWidget* Window);
	
	//강화부분  추가
	/** NPC 서비스처럼 하단 메뉴와 별개인 창을 WindowLayer에 표시합니다. */
	bool ShowServiceWindow(UTDWindowBaseWidget* Window);

	/** 게임용 창을 표시할 수 있는 상태인지 확인합니다. */
	bool IsGameplayWindowLayerReady() const;

	/** 현재 WindowLayer에 표시 중인 창이 있는지 확인합니다. */
	bool HasVisibleGameWindow() const;

private:
	UPROPERTY(Transient)
		TSubclassOf<UTDLoginWidget> AccountScreenClass;
	UPROPERTY(Transient)
		TObjectPtr<UTDLoginWidget> AccountScreen;
	FTimerHandle AccountFlowTimer;
	void RefreshAccountFlow();

	struct FWindowPlacement
	{
		FVector2D AnchorMinimum = FVector2D::ZeroVector;
		FVector2D AnchorMaximum = FVector2D::ZeroVector;
		FVector2D Alignment = FVector2D::ZeroVector;
		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector;
		bool bAutoSize = false;
	};

	TWeakObjectPtr<UTDUIRootWidget> RootWidget;
	TMap<ETDNavMenuType, TWeakObjectPtr<UTDWindowBaseWidget>> OpenWindows;
	TMap<ETDNavMenuType, TWeakObjectPtr<UTDWindowBaseWidget>> PrecreatedWindows;
	TMap<ETDNavMenuType, FWindowPlacement> SavedWindowPlacements;

	void ToggleWindow(ETDNavMenuType MenuType);
	TSubclassOf<UTDWindowBaseWidget> ResolveWindowClass(ETDNavMenuType MenuType);
	void SaveWindowPosition(ETDNavMenuType MenuType, UTDWindowBaseWidget* Window);
	void RestoreWindowPosition(ETDNavMenuType MenuType, UTDWindowBaseWidget* Window) const;
	void ClearWindowRegistry();

	UFUNCTION()
		void HandleWindowClosed(UTDWindowBaseWidget* ClosedWindow);
};
