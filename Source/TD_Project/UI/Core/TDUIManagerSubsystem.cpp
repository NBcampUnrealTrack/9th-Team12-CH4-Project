#include "UI/Core/TDUIManagerSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "UI/TEST/TDLoginWidget.h"
#include "UI/TEST/TDUI_Login_PlayerController.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UI/Core/TDUIRootWidget.h"
#include "UI/Layer/WindowLayer/Common/WindowBase/TDWindowBaseWidget.h"
#include "UI/Settings/TDUISettings.h"

void UTDUIManagerSubsystem::Deinitialize()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AccountFlowTimer);
    AccountScreen = nullptr;
	ClearWindowRegistry();
	SavedWindowPlacements.Empty();
	RootWidget.Reset();

	Super::Deinitialize();
}

void UTDUIManagerSubsystem::RegisterRoot(UTDUIRootWidget* InRootWidget)
{
	if (!IsValid(InRootWidget) || RootWidget.Get() == InRootWidget)
	{
		return;
	}

	ClearWindowRegistry();
	RootWidget = InRootWidget;
    AccountScreen = nullptr;
    RefreshAccountFlow();

	// 현재 WBP_Root에 미리 배치된 인벤토리 창이 있으면 첫 프레임부터
	// 보이지 않게 숨긴 뒤, 첫 Nav 클릭에서 같은 인스턴스를 재사용한다.
	UCanvasPanel* WindowLayer = InRootWidget->GetWindowLayer();
	UClass* ResolvedInventoryClass = ResolveWindowClass(ETDNavMenuType::Inventory);
	if (!WindowLayer || !ResolvedInventoryClass)
	{
		return;
	}

	for (int32 ChildIndex = 0; ChildIndex < WindowLayer->GetChildrenCount(); ++ChildIndex)
	{
		UTDWindowBaseWidget* ExistingWindow =
			Cast<UTDWindowBaseWidget>(WindowLayer->GetChildAt(ChildIndex));
		if (!IsValid(ExistingWindow) || !ExistingWindow->IsA(ResolvedInventoryClass))
		{
			continue;
		}

		ExistingWindow->SetVisibility(ESlateVisibility::Collapsed);
		ExistingWindow->OnWindowClosed.AddUniqueDynamic(
			this,
			&ThisClass::HandleWindowClosed);
		PrecreatedWindows.Add(ETDNavMenuType::Inventory, ExistingWindow);
		break;
	}
}

void UTDUIManagerSubsystem::UnregisterRoot(UTDUIRootWidget* InRootWidget)
{
	if (RootWidget.Get() != InRootWidget)
	{
		return;
	}

	ClearWindowRegistry();
	RootWidget.Reset();
    AccountScreen = nullptr;
}

void UTDUIManagerSubsystem::RequestMenu(ETDNavMenuType MenuType)
{
	switch (MenuType)
	{
	case ETDNavMenuType::Inventory:
	case ETDNavMenuType::Character:
	case ETDNavMenuType::System:
		ToggleWindow(MenuType);
		break;

	default:
		UE_LOG(LogTemp, Verbose,
			TEXT("UI: 아직 연결되지 않은 메뉴 버튼입니다. (MenuType: %d)"),
			static_cast<int32>(MenuType));
		break;
	}
}

bool UTDUIManagerSubsystem::IsMenuOpen(ETDNavMenuType MenuType) const
{
	const TWeakObjectPtr<UTDWindowBaseWidget>* FoundWindow = OpenWindows.Find(MenuType);
	const UTDWindowBaseWidget* Window = FoundWindow ? FoundWindow->Get() : nullptr;
	return IsValid(Window) && Window->GetParent() != nullptr
		&& Window->GetVisibility() != ESlateVisibility::Collapsed;
}

void UTDUIManagerSubsystem::BringWindowToFront(UTDWindowBaseWidget* Window)
{
    UTDUIRootWidget* Root = RootWidget.Get();
    UCanvasPanel* Layer = Root ? Root->GetWindowLayer() : nullptr;
    UCanvasPanelSlot* SelectedSlot = IsValid(Window) ? Cast<UCanvasPanelSlot>(Window->Slot) : nullptr;
    if (!Layer || !SelectedSlot || Window->GetParent() != Layer)
    {
        return;
    }

    TArray<UCanvasPanelSlot*> OtherSlots;
    for (int32 Index = 0; Index < Layer->GetChildrenCount(); ++Index)
    {
        UTDWindowBaseWidget* Child = Cast<UTDWindowBaseWidget>(Layer->GetChildAt(Index));
        if (Child && Child != Window)
        {
            if (UCanvasPanelSlot* ChildSlot = Cast<UCanvasPanelSlot>(Child->Slot))
            {
                OtherSlots.Add(ChildSlot);
            }
        }
    }

    // 같은 ZOrder에서는 기존 자식 순서를 유지하고, 값이 계속 커지지 않도록 정리한다.
    OtherSlots.StableSort([](const UCanvasPanelSlot& A, const UCanvasPanelSlot& B)
    {
        return A.GetZOrder() < B.GetZOrder();
    });
    for (int32 Index = 0; Index < OtherSlots.Num(); ++Index)
    {
        OtherSlots[Index]->SetZOrder(Index);
    }
    SelectedSlot->SetZOrder(OtherSlots.Num());
}

void UTDUIManagerSubsystem::ToggleWindow(ETDNavMenuType MenuType)
{
	// 삼항으로 두면 메뉴가 늘 때마다 "인벤토리" 로 잘못 찍힌다. 경고 문구가 틀리면
	// 원인을 엉뚱한 곳에서 찾게 되므로 switch 로 바꾼다.
	const TCHAR* MenuName = TEXT("인벤토리");
	const TCHAR* SettingName = TEXT("Inventory Window Class");

	switch (MenuType)
	{
	case ETDNavMenuType::Character:
		MenuName = TEXT("캐릭터 정보");
		SettingName = TEXT("Character Window Class");
		break;

	case ETDNavMenuType::System:
		MenuName = TEXT("설정");
		SettingName = TEXT("System Window Class");
		break;

	default:
		break;
	}

	if (TWeakObjectPtr<UTDWindowBaseWidget>* FoundWindow = OpenWindows.Find(MenuType))
	{
		if (UTDWindowBaseWidget* OpenWindow = FoundWindow->Get())
		{
			OpenWindow->CloseWindow();
			return;
		}

		OpenWindows.Remove(MenuType);
	}

	UTDUIRootWidget* Root = RootWidget.Get();
	if (!IsValid(Root))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UI: %s 창을 열 수 없습니다. WBP_Root가 UI 관리자에 등록되지 않았습니다."), MenuName);
		return;
	}

	if (TWeakObjectPtr<UTDWindowBaseWidget>* PrecreatedWindow = PrecreatedWindows.Find(MenuType))
	{
		if (UTDWindowBaseWidget* Window = PrecreatedWindow->Get())
		{
			RestoreWindowPosition(MenuType, Window);
			Window->SetVisibility(ESlateVisibility::Visible);
			OpenWindows.Add(MenuType, Window);
			BringWindowToFront(Window);
			OnMenuWindowStateChanged.Broadcast(MenuType, true);
			return;
		}

		PrecreatedWindows.Remove(MenuType);
	}

	TSubclassOf<UTDWindowBaseWidget> WindowClass = ResolveWindowClass(MenuType);
	if (!WindowClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UI: %s 위젯이 설정되지 않았습니다. 프로젝트 설정 > Game > TD UI > %s를 지정하세요."), MenuName, SettingName);
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	APlayerController* PlayerController =
		LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UI: %s 창을 열 수 없습니다. 현재 로컬 플레이어를 찾지 못했습니다."), MenuName);
		return;
	}

	UTDWindowBaseWidget* NewWindow =
		CreateWidget<UTDWindowBaseWidget>(PlayerController, WindowClass);
	if (!IsValid(NewWindow))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UI: %s 위젯 생성에 실패했습니다. %s 설정을 확인하세요."), MenuName, SettingName);
		return;
	}

	if (!Root->AddWindow(NewWindow))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UI: %s 창을 표시할 수 없습니다. WBP_Root의 WindowLayer 연결을 확인하세요."), MenuName);
		return;
	}

	RestoreWindowPosition(MenuType, NewWindow);

	NewWindow->OnWindowClosed.AddUniqueDynamic(
		this,
		&ThisClass::HandleWindowClosed);
	OpenWindows.Add(MenuType, NewWindow);
	BringWindowToFront(NewWindow);
	OnMenuWindowStateChanged.Broadcast(MenuType, true);
}

void UTDUIManagerSubsystem::SaveWindowPosition(
	ETDNavMenuType MenuType,
	UTDWindowBaseWidget* Window)
{
	if (IsValid(Window))
	{
		if (const UCanvasPanelSlot* WindowSlot = Cast<UCanvasPanelSlot>(Window->Slot))
		{
			const FAnchors Anchors = WindowSlot->GetAnchors();

			FWindowPlacement Placement;
			Placement.AnchorMinimum = Anchors.Minimum;
			Placement.AnchorMaximum = Anchors.Maximum;
			Placement.Alignment = WindowSlot->GetAlignment();
			Placement.Position = WindowSlot->GetPosition();
			Placement.Size = WindowSlot->GetSize();
			Placement.bAutoSize = WindowSlot->GetAutoSize();
			SavedWindowPlacements.Add(MenuType, Placement);
		}
	}
}

void UTDUIManagerSubsystem::RestoreWindowPosition(
	ETDNavMenuType MenuType,
	UTDWindowBaseWidget* Window) const
{
	const FWindowPlacement* Placement = SavedWindowPlacements.Find(MenuType);
	if (Placement && IsValid(Window))
	{
		if (UCanvasPanelSlot* WindowSlot = Cast<UCanvasPanelSlot>(Window->Slot))
		{
			FAnchors RestoredAnchors;
			RestoredAnchors.Minimum = Placement->AnchorMinimum;
			RestoredAnchors.Maximum = Placement->AnchorMaximum;
			WindowSlot->SetAnchors(RestoredAnchors);
			WindowSlot->SetAlignment(Placement->Alignment);
			WindowSlot->SetAutoSize(Placement->bAutoSize);
			WindowSlot->SetPosition(Placement->Position);

			if (!Placement->bAutoSize)
			{
				WindowSlot->SetSize(Placement->Size);
			}
		}
	}
}

TSubclassOf<UTDWindowBaseWidget> UTDUIManagerSubsystem::ResolveWindowClass(
	ETDNavMenuType MenuType)
{
	switch (MenuType)
	{
	case ETDNavMenuType::Inventory:
		if (const UTDUISettings* UISettings = GetDefault<UTDUISettings>())
		{
			return UISettings->InventoryWindowClass.LoadSynchronous();
		}
		return nullptr;

	case ETDNavMenuType::Character:
		if (const UTDUISettings* UISettings = GetDefault<UTDUISettings>())
		{
			return UISettings->CharacterWindowClass.LoadSynchronous();
		}
		return nullptr;

	case ETDNavMenuType::System:
		if (const UTDUISettings* UISettings = GetDefault<UTDUISettings>())
		{
			return UISettings->SystemWindowClass.LoadSynchronous();
		}
		return nullptr;

	default:
		return nullptr;
	}
}

void UTDUIManagerSubsystem::ClearWindowRegistry()
{
	auto RemoveClosedDelegate = [this](
		TMap<ETDNavMenuType, TWeakObjectPtr<UTDWindowBaseWidget>>& Windows)
	{
		for (TPair<ETDNavMenuType, TWeakObjectPtr<UTDWindowBaseWidget>>& Pair : Windows)
		{
			if (UTDWindowBaseWidget* Window = Pair.Value.Get())
			{
				Window->OnWindowClosed.RemoveDynamic(
					this,
					&ThisClass::HandleWindowClosed);
			}
		}
		Windows.Empty();
	};

	RemoveClosedDelegate(OpenWindows);
	RemoveClosedDelegate(PrecreatedWindows);
}

void UTDUIManagerSubsystem::HandleWindowClosed(UTDWindowBaseWidget* ClosedWindow)
{
	if (!IsValid(ClosedWindow))
	{
		return;
	}

	for (auto Iterator = OpenWindows.CreateIterator(); Iterator; ++Iterator)
	{
		if (Iterator.Value().Get() != ClosedWindow)
		{
			continue;
		}

		const ETDNavMenuType ClosedMenuType = Iterator.Key();
		SaveWindowPosition(ClosedMenuType, ClosedWindow);
		Iterator.RemoveCurrent();
		PrecreatedWindows.Remove(ClosedMenuType);
		OnMenuWindowStateChanged.Broadcast(ClosedMenuType, false);
		break;
	}
}

void UTDUIManagerSubsystem::StartAccountFlow(TSubclassOf<UTDLoginWidget> WidgetClass)
{
    AccountScreenClass = WidgetClass;
    if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(AccountFlowTimer, this, &ThisClass::RefreshAccountFlow, .1f, true);
    RefreshAccountFlow();
}

void UTDUIManagerSubsystem::RefreshAccountFlow()
{
    UTDUIRootWidget* Root = RootWidget.Get();
    if (!Root || !AccountScreenClass || !Root->GetScreenStack()) return;
    APlayerController* Controller = Root->GetOwningPlayer();
    if (!Controller || !Controller->IsLocalController()) return;
    Root->RefreshPlayerHUD();
    UCommonActivatableWidgetStack* Stack = Root->GetScreenStack();
    if (Root->IsPlayerHUDReady())
    {
        if (AccountScreen)
        {
            Stack->RemoveWidget(*AccountScreen);
            AccountScreen = nullptr;
            Controller->SetInputMode(FInputModeGameOnly());
            Controller->bShowMouseCursor = false;
            if (Root->GetWindowLayer()) Root->GetWindowLayer()->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        return;
    }
    if (!AccountScreen)
    {
        // 이전 캐릭터의 창/팝업을 다음 캐릭터에게 남기지 않는다.
        TArray<TWeakObjectPtr<UTDWindowBaseWidget>> Windows;
        OpenWindows.GenerateValueArray(Windows);
        for (auto Window : Windows) if (Window.IsValid()) Window->CloseWindow();
        Root->GetModalStack()->ClearWidgets();
        Stack->ClearWidgets();
        AccountScreen = Stack->AddWidget<UTDLoginWidget>(AccountScreenClass);
        if (Root->GetWindowLayer()) Root->GetWindowLayer()->SetVisibility(ESlateVisibility::Collapsed);
        if (AccountScreen)
        {
            FInputModeUIOnly Mode;
            Mode.SetWidgetToFocus(AccountScreen->TakeWidget());
            Controller->SetInputMode(Mode);
            Controller->bShowMouseCursor = true;
        }
    }
}

void UTDUIManagerSubsystem::RequestCharacterSelection()
{
    if (ATDUI_Login_PlayerController* Controller = Cast<ATDUI_Login_PlayerController>(GetLocalPlayer()->GetPlayerController(GetWorld()))) Controller->TDCharacterSelect();
}

void UTDUIManagerSubsystem::RequestLogout()
{
    if (ATDUI_Login_PlayerController* Controller = Cast<ATDUI_Login_PlayerController>(GetLocalPlayer()->GetPlayerController(GetWorld()))) Controller->TDLogout();
}
