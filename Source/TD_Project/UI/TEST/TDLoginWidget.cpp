#include "UI/TEST/TDLoginWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Player/TDPlayerState.h"
#include "UI/TEST/TDUI_Login_PlayerController.h"
#include "Data/TDCharacterClassRow.h"
#include "Settings/TDCharacterClassSettings.h"
#include "Engine/DataTable.h"
#include "Input/CommonUIInputTypes.h"

void UTDLoginWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SlotButtons = {SlotButton0, SlotButton1, SlotButton2, SlotButton3, SlotButton4, SlotButton5};
    SlotLabels = {SlotLabel0, SlotLabel1, SlotLabel2, SlotLabel3, SlotLabel4, SlotLabel5};
    LoginButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Login);
    SlotButton0->OnClicked.AddUniqueDynamic(this, &ThisClass::Select0);
    SlotButton1->OnClicked.AddUniqueDynamic(this, &ThisClass::Select1);
    SlotButton2->OnClicked.AddUniqueDynamic(this, &ThisClass::Select2);
    SlotButton3->OnClicked.AddUniqueDynamic(this, &ThisClass::Select3);
    SlotButton4->OnClicked.AddUniqueDynamic(this, &ThisClass::Select4);
    SlotButton5->OnClicked.AddUniqueDynamic(this, &ThisClass::Select5);
    Refresh();
}

void UTDLoginWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	RefreshElapsed += DeltaTime;
	if (RefreshElapsed >= .1f) { RefreshElapsed = 0.f; Refresh(); }
}

void UTDLoginWidget::Refresh()
{
	APlayerController* Controller = GetOwningPlayer();
	ATDPlayerState* Player = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;
	if (!Player) { Status->SetText(WaitingMessage); return; }
	const ATDUI_Login_PlayerController* Dummy = Cast<ATDUI_Login_PlayerController>(Controller);
	if (!Dummy) { Status->SetText(WrongControllerMessage); return; }
	const bool bLoggedIn = Dummy->IsLoggedIn();
	LoginPanel->SetVisibility(bLoggedIn ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	CharacterPanel->SetVisibility(bLoggedIn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Status->SetText(FText::FromString(Dummy->GetAccountMessage()));
	const auto& Slots = Player->GetCharacterSlots();
	for (int32 Index = 0; Index < SlotButtons.Num(); ++Index)
	{
		const bool bExists = Slots.IsValidIndex(Index);
		SlotButtons[Index]->SetIsEnabled(bExists && !Player->HasSelectedCharacter());
        FText Text = FText::Format(EmptySlotFormat, FText::AsNumber(Index + 1));
        if (bExists)
        {
            const auto& Info = Slots[Index];
            FText ClassName = FText::FromName(Info.ClassId);
            if (UDataTable* Table = GetDefault<UTDCharacterClassSettings>()->ClassTable.LoadSynchronous())
            {
                if (const auto* Row = Table->FindRow<FTDCharacterClassRow>(Info.ClassId, TEXT("DummyLogin"))) ClassName = Row->DisplayName;
            }
            Text = FText::Format(CharacterSlotFormat, FText::AsNumber(Index + 1), FText::FromString(Info.CharacterName), ClassName, FText::AsNumber(Info.Level));
        }
        SlotLabels[Index]->SetText(Text);
	}
}

void UTDLoginWidget::Login()
{
	if (ATDUI_Login_PlayerController* Player = Cast<ATDUI_Login_PlayerController>(GetOwningPlayer()))
	{
		Player->ServerLogin(LoginInput->GetText().ToString(), PasswordInput->GetText().ToString());
		PasswordInput->SetText(FText::GetEmpty());
	}
}
void UTDLoginWidget::Select(int32 SlotIndex)
{
	if (ATDUI_Login_PlayerController* Player = Cast<ATDUI_Login_PlayerController>(GetOwningPlayer())) Player->ServerSelectCharacter(SlotIndex);
}
void UTDLoginWidget::Select0() { Select(0); }
void UTDLoginWidget::Select1() { Select(1); }
void UTDLoginWidget::Select2() { Select(2); }
void UTDLoginWidget::Select3() { Select(3); }
void UTDLoginWidget::Select4() { Select(4); }
void UTDLoginWidget::Select5() { Select(5); }

TOptional<FUIInputConfig> UTDLoginWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}
