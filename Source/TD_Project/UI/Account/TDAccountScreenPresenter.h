#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TDAccountScreenPresenter.generated.h"

class UUserWidget;
class ATDPlayerController;

/** Account screen presentation only. Mutations remain in the existing server RPCs. */
UCLASS()
class TD_PROJECT_API UTDAccountScreenPresenter : public UObject
{
    GENERATED_BODY()
public:
    bool Initialize(UUserWidget* InHost);
    void Refresh();
    void Shutdown();
private:
    UPROPERTY() TArray<TObjectPtr<UUserWidget>> Pages;
    TWeakObjectPtr<UUserWidget> Host;
    int32 Page = 0;
    int32 SelectedSlot = INDEX_NONE;
    int32 ReplySerial = 0;
    bool bPending = false;
    bool bDeleteOpen = false;
    bool bFilteringPasswordText = false;
    FGuid DeleteTarget;
    FString DeleteTargetName;
    bool bLoggedIn = false;
    FName PendingAction;
    FName SelectedClass = TEXT("Warrior");
    FText Message;
    FString RosterKey;
    void Show(int32 Index);
    void SelectSlot(int32 Index);
    void SelectClass(FName ClassId);
    void RenderRoster();
    ATDPlayerController* Controller() const;
    UFUNCTION() void Login();
    UFUNCTION() void Register();
    UFUNCTION() void FilterPasswordText(const FText& Text);
    UFUNCTION() void OpenRegister();
    UFUNCTION() void BackToLogin();
    UFUNCTION() void OpenCreate();
    UFUNCTION() void BackToSelect();
    UFUNCTION() void CreateCharacter();
    UFUNCTION() void StartGame();
    UFUNCTION() void Logout();
    UFUNCTION() void OpenDelete();
    UFUNCTION() void ConfirmDelete();
    UFUNCTION() void CancelDelete();
    UFUNCTION() void Warrior();
    UFUNCTION() void Mage();
    UFUNCTION() void Archer();
    UFUNCTION() void Slot0();
    UFUNCTION() void Slot1();
    UFUNCTION() void Slot2();
    UFUNCTION() void Slot3();
    UFUNCTION() void Slot4();
    UFUNCTION() void Slot5();
};
