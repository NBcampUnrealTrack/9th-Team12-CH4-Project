#include "UI/TEST/TDLoginWidget.h"
#include "UI/Account/TDAccountScreenPresenter.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Character/TDCharacterClassData.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Player/TDPlayerState.h"
#include "Player/TDPlayerController.h"
#include "Data/TDCharacterClassRow.h"
#include "Settings/TDCharacterClassSettings.h"
#include "Engine/DataTable.h"
#include "Input/CommonUIInputTypes.h"

void UTDLoginWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (AccountPresenter) AccountPresenter->Shutdown();
    AccountPresenter = NewObject<UTDAccountScreenPresenter>(this);
    if (AccountPresenter->Initialize(this)) return;
    AccountPresenter = nullptr;
    SlotButtons = {SlotButton0, SlotButton1, SlotButton2, SlotButton3, SlotButton4, SlotButton5};
    SlotLabels = {SlotLabel0, SlotLabel1, SlotLabel2, SlotLabel3, SlotLabel4, SlotLabel5};
    LoginButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Login);
    SlotButton0->OnClicked.AddUniqueDynamic(this, &ThisClass::Select0);
    SlotButton1->OnClicked.AddUniqueDynamic(this, &ThisClass::Select1);
    SlotButton2->OnClicked.AddUniqueDynamic(this, &ThisClass::Select2);
    SlotButton3->OnClicked.AddUniqueDynamic(this, &ThisClass::Select3);
    SlotButton4->OnClicked.AddUniqueDynamic(this, &ThisClass::Select4);
    SlotButton5->OnClicked.AddUniqueDynamic(this, &ThisClass::Select5);
    if (OpenRegisterButton) OpenRegisterButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenRegister);
    if (SubmitRegisterButton) SubmitRegisterButton->OnClicked.AddUniqueDynamic(this, &ThisClass::SubmitRegister);
    if (RegisterBackButton) RegisterBackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Back);
    if (OpenCreateButton) OpenCreateButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenCreate);
    if (SubmitCreateButton) SubmitCreateButton->OnClicked.AddUniqueDynamic(this, &ThisClass::SubmitCreate);
    if (CreateBackButton) CreateBackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Back);
    if (StartGameButton) StartGameButton->OnClicked.AddUniqueDynamic(this, &ThisClass::StartGame);
    if (auto* PC = Cast<ATDPlayerController>(GetOwningPlayer())) SeenReply = PC->GetAccountReplySerial();
    if (ClassPicker)
    {
        ClassPicker->ClearOptions(); ClassIds.Reset();
        if (auto* Table = GetDefault<UTDCharacterClassSettings>()->ClassTable.LoadSynchronous())
        {
            auto Names = Table->GetRowNames(); Names.Sort(FNameLexicalLess());
            for (FName Id : Names)
                if (auto* Row = Table->FindRow<FTDCharacterClassRow>(Id, TEXT("AccountClassPicker")))
                { ClassIds.Add(Id); ClassPicker->AddOption(Row->DisplayName.IsEmpty() ? Id.ToString() : Row->DisplayName.ToString()); }
        }
        ClassPicker->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::ClassChanged);
        if (ClassIds.Num()) ClassPicker->SetSelectedIndex(0);
    }
    Refresh();
}

void UTDLoginWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	RefreshElapsed += DeltaTime;
	if (RefreshElapsed >= .1f) { RefreshElapsed = 0.f; if (AccountPresenter) AccountPresenter->Refresh(); else Refresh(); }
}

void UTDLoginWidget::Refresh()
{
	APlayerController* Controller = GetOwningPlayer();
	ATDPlayerState* Player = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;
	if (!Player) { Status->SetText(WaitingMessage); return; }
	const ATDPlayerController* Dummy = Cast<ATDPlayerController>(Controller);
	if (!Dummy) { Status->SetText(WrongControllerMessage); return; }
	const bool bLoggedIn = Dummy->IsLoggedIn();
    if (SeenReply != Dummy->GetAccountReplySerial())
    {
        SeenReply = Dummy->GetAccountReplySerial(); LocalMessage = FText::GetEmpty(); bPending = false;
        if (Dummy->GetAccountReplyAction() == PendingAction && Dummy->WasAccountActionSuccessful())
        {
            if (PendingAction == TEXT("Register")) bRegisterPage = false;
            if (PendingAction == TEXT("Create")) { bCreatePage = false; SelectedSlot = INDEX_NONE; }
        }
        PendingAction = NAME_None;
    }
    if (bWasLoggedIn != bLoggedIn)
    { bRegisterPage = false; bCreatePage = false; SelectedSlot = INDEX_NONE; PreviewClass = NAME_None; bPending = false; }
    bWasLoggedIn = bLoggedIn;
    LoginPanel->SetVisibility(!bLoggedIn && !bRegisterPage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    CharacterPanel->SetVisibility(bLoggedIn && !bCreatePage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (RegisterPanel) RegisterPanel->SetVisibility(!bLoggedIn && bRegisterPage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (CreatePanel) CreatePanel->SetVisibility(bLoggedIn && bCreatePage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    Status->SetText(LocalMessage.IsEmpty() ? FText::FromString(Dummy->GetAccountMessage()) : LocalMessage);
    LoginButton->SetIsEnabled(!bPending);
    if (SubmitRegisterButton) SubmitRegisterButton->SetIsEnabled(!bPending);
    if (SubmitCreateButton) SubmitCreateButton->SetIsEnabled(!bPending && ClassIds.Num() > 0);
    if (OpenCreateButton) OpenCreateButton->SetIsEnabled(!bPending && Player->GetCharacterSlots().Num() < 6);
    if (StartGameButton) StartGameButton->SetIsEnabled(!bPending && Player->GetCharacterSlots().IsValidIndex(SelectedSlot) && !Player->HasSelectedCharacter());
	const auto& Slots = Player->GetCharacterSlots();
    if (!Slots.IsValidIndex(SelectedSlot))
    {
        SelectedSlot = INDEX_NONE; PreviewClass = NAME_None;
        if (SelectionPreview) SelectionPreview->SetRenderOpacity(0.f);
        if (SelectionText) SelectionText->SetText(FText::FromString(TEXT("캐릭터를 선택해 주세요.")));
    }
	for (int32 Index = 0; Index < SlotButtons.Num(); ++Index)
	{
		const bool bExists = Slots.IsValidIndex(Index);
		SlotButtons[Index]->SetIsEnabled(!bPending && !Player->HasSelectedCharacter() && (bExists || OpenCreateButton != nullptr));
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
        SlotButtons[Index]->SetBackgroundColor(Index == SelectedSlot ? FLinearColor(0.45f, 0.65f, 0.9f) : FLinearColor::White);
        if (bExists && Index == SelectedSlot)
        {
            if (SelectionText) SelectionText->SetText(Text);
            if (PreviewClass != Slots[Index].ClassId) { PreviewClass = Slots[Index].ClassId; UpdatePreview(SelectionPreview, PreviewClass); }
        }
	}
}

void UTDLoginWidget::Login()
{
	if (ATDPlayerController* Player = Cast<ATDPlayerController>(GetOwningPlayer()))
	{
        if (bPending) return;
        bPending = true; LocalMessage = FText::FromString(TEXT("로그인 중..."));
		Player->ServerLogin(LoginInput->GetText().ToString().TrimStartAndEnd(), PasswordInput->GetText().ToString());
		PasswordInput->SetText(FText::GetEmpty());
	}
}
void UTDLoginWidget::Select(int32 SlotIndex)
{
    auto* PC = Cast<ATDPlayerController>(GetOwningPlayer());
    auto* State = PC ? PC->GetPlayerState<ATDPlayerState>() : nullptr;
    if (bPending || !State) return;
    if (!State->GetCharacterSlots().IsValidIndex(SlotIndex)) { OpenCreate(); return; }
    SelectedSlot = SlotIndex;
    if (!StartGameButton) PC->ServerSelectCharacter(SlotIndex);
    Refresh();
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

void UTDLoginWidget::OpenRegister() { if (!bPending && RegisterPanel) { bRegisterPage = true; LocalMessage = FText::GetEmpty(); Refresh(); } }
void UTDLoginWidget::OpenCreate() { if (!bPending && CreatePanel) { bCreatePage = true; LocalMessage = FText::GetEmpty(); Refresh(); } }
void UTDLoginWidget::Back()
{
    if (bPending) return;
    bRegisterPage = false; bCreatePage = false; LocalMessage = FText::GetEmpty();
    if (RegisterPasswordInput) RegisterPasswordInput->SetText(FText::GetEmpty());
    if (ConfirmPasswordInput) ConfirmPasswordInput->SetText(FText::GetEmpty());
    Refresh();
}
void UTDLoginWidget::SubmitRegister()
{
    auto* PC = Cast<ATDPlayerController>(GetOwningPlayer());
    if (!PC || bPending || !RegisterLoginInput || !RegisterPasswordInput || !ConfirmPasswordInput) return;
    const FString Password = RegisterPasswordInput->GetText().ToString();
    if (Password != ConfirmPasswordInput->GetText().ToString())
    { LocalMessage = FText::FromString(TEXT("비밀번호가 일치하지 않습니다.")); Refresh(); return; }
    LoginInput->SetText(RegisterLoginInput->GetText());
    bPending = true; PendingAction = TEXT("Register"); LocalMessage = FText::FromString(TEXT("가입 처리 중..."));
    PC->ServerRegisterAccount(RegisterLoginInput->GetText().ToString().TrimStartAndEnd(), Password);
    RegisterPasswordInput->SetText(FText::GetEmpty()); ConfirmPasswordInput->SetText(FText::GetEmpty()); Refresh();
}
void UTDLoginWidget::SubmitCreate()
{
    auto* PC = Cast<ATDPlayerController>(GetOwningPlayer());
    const int32 Index = ClassPicker ? ClassPicker->GetSelectedIndex() : INDEX_NONE;
    if (!PC || bPending || !CharacterNameInput || !ClassIds.IsValidIndex(Index)) return;
    bPending = true; PendingAction = TEXT("Create"); LocalMessage = FText::FromString(TEXT("캐릭터 생성 중..."));
    PC->ServerCreateCharacter(CharacterNameInput->GetText().ToString().TrimStartAndEnd(), ClassIds[Index]); Refresh();
}
void UTDLoginWidget::StartGame()
{
    if (auto* PC = Cast<ATDPlayerController>(GetOwningPlayer()))
    {
        if (bPending || SelectedSlot == INDEX_NONE) return;
        bPending = true; LocalMessage = FText::FromString(TEXT("월드에 입장하는 중..."));
        PC->ServerSelectCharacter(SelectedSlot); Refresh();
    }
}
void UTDLoginWidget::ClassChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    const int32 Index = ClassPicker ? ClassPicker->GetSelectedIndex() : INDEX_NONE;
    UpdatePreview(ClassPreview, ClassIds.IsValidIndex(Index) ? ClassIds[Index] : NAME_None);
}
void UTDLoginWidget::UpdatePreview(UImage* Image, FName ClassId)
{
    if (!Image) return;
    UTexture2D* Texture = nullptr;
    if (auto* Table = GetDefault<UTDCharacterClassSettings>()->ClassTable.LoadSynchronous())
        if (auto* Row = Table->FindRow<FTDCharacterClassRow>(ClassId, TEXT("AccountPreview")))
            if (auto* Visual = Row->VisualData.LoadSynchronous())
            { Texture = Visual->FullBody.LoadSynchronous(); if (!Texture) Texture = Visual->Portrait.LoadSynchronous(); }
    Image->SetRenderOpacity(Texture ? 1.f : 0.f);
    Image->SetBrushFromTexture(Texture);
    Image->SetVisibility(Texture ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UTDLoginWidget::NativeDestruct()
{
    if (AccountPresenter) AccountPresenter->Shutdown();
    AccountPresenter = nullptr;
    Super::NativeDestruct();
}
