#include "TDAccountScreenPresenter.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Core/TDAccountSubSystem.h"
#include "Player/TDPlayerController.h"
#include "Player/TDPlayerState.h"
#include "Settings/TDCharacterClassSettings.h"
#include "Data/TDCharacterClassRow.h"
#include "Character/TDCharacterClassData.h"
#include "Engine/DataTable.h"

namespace
{
    template<class T> T* Find(UUserWidget* Page, const FString& Name)
    { return Page && Page->WidgetTree ? Cast<T>(Page->WidgetTree->FindWidget(FName(*("Account_" + Name)))) : nullptr; }
    void Text(UUserWidget* Page, const FString& Name, const FText& Value)
    { if (auto* W = Find<UTextBlock>(Page, Name)) W->SetText(Value); }
    const FTDCharacterClassRow* ClassRow(FName ClassId)
    {
        auto* Table = GetDefault<UTDCharacterClassSettings>()->ClassTable.LoadSynchronous();
        return Table ? Table->FindRow<FTDCharacterClassRow>(ClassId, TEXT("AccountScreen")) : nullptr;
    }
    void Portrait(UUserWidget* Page, const FString& Name, FName ClassId, bool bUseIcon = false)
    {
        auto* Image = Find<UImage>(Page, Name); if (!Image) return;
        UTexture2D* Texture = nullptr;
        if (const auto* Row = ClassRow(ClassId))
            if (auto* Visual = Row->VisualData.LoadSynchronous()) Texture = bUseIcon ? Visual->Icon.LoadSynchronous() : Visual->FullBody.LoadSynchronous();
        Image->SetBrushFromTexture(Texture, true);
        Image->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
}

ATDPlayerController* UTDAccountScreenPresenter::Controller() const
{ return Host.IsValid() ? Cast<ATDPlayerController>(Host->GetOwningPlayer()) : nullptr; }

bool UTDAccountScreenPresenter::Initialize(UUserWidget* InHost)
{
    Host = InHost;
    auto* PC = Controller();
    if (!PC || !InHost || !InHost->WidgetTree) return false;
    // 화면의 생성과 배치는 WBP가 소유하고 C++은 배치된 화면에만 연결한다.
    for (const TCHAR* Name : {TEXT("LoginPage"), TEXT("RegisterPage"), TEXT("SelectPage"), TEXT("CreatePage")})
    {
        auto* Widget = Find<UUserWidget>(InHost, Name);
        if (!Widget) { Shutdown(); return false; }
        Pages.Add(Widget);
    }
    bool bValid = true;
#define BIND(P, N, F) if (auto* B = Find<UButton>(Pages[P], TEXT(N))) { B->OnClicked.AddUniqueDynamic(this, &ThisClass::F); } else { bValid = false; }
    BIND(0,"SubmitLogin",Login); BIND(0,"OpenRegister",OpenRegister);
    BIND(1,"SubmitRegister",Register); BIND(1,"BackToLogin",BackToLogin);
    BIND(2,"OpenDelete",OpenDelete); BIND(2,"ConfirmDelete",ConfirmDelete); BIND(2,"CancelDelete",CancelDelete);
    BIND(2,"StartGame",StartGame); BIND(2,"OpenCreate",OpenCreate); BIND(2,"Back",Logout);
    BIND(2,"Slot0Button",Slot0); BIND(2,"Slot1Button",Slot1); BIND(2,"Slot2Button",Slot2);
    BIND(2,"Slot3Button",Slot3); BIND(2,"Slot4Button",Slot4); BIND(2,"Slot5Button",Slot5);
    BIND(3,"CreateCharacter",CreateCharacter); BIND(3,"BackToSelect",BackToSelect); BIND(3,"Back",BackToSelect);
    BIND(3,"ChooseWarrior",Warrior); BIND(3,"ChooseMage",Mage); BIND(3,"ChooseArcher",Archer);
#undef BIND
    for (int32 P : {0,1}) bValid &= Find<UEditableTextBox>(Pages[P], TEXT("LoginId")) && Find<UEditableTextBox>(Pages[P], TEXT("Password"));
    bValid &= Find<UEditableTextBox>(Pages[1], TEXT("ConfirmPassword")) && Find<UEditableTextBox>(Pages[3], TEXT("CharacterName"));
    bValid &= Find<UCanvasPanel>(Pages[2], TEXT("DeletePopup")) && Find<UTextBlock>(Pages[2], TEXT("DeleteMessage"));
    if (!bValid) { UE_LOG(LogTemp, Error, TEXT("Account screen widget names do not match the presenter.")); Shutdown(); return false; }
    for (int32 Index = 0; Index < 6; ++Index)
    {
        const FString Prefix = FString::Printf(TEXT("Slot%d"), Index);
        bValid &= Find<UImage>(Pages[2], Prefix + TEXT("FullBody")) != nullptr;
        bValid &= Find<UTextBlock>(Pages[2], Prefix + TEXT("Label")) != nullptr;
        bValid &= Find<UWidget>(Pages[2], Prefix + TEXT("ButtonSelected")) != nullptr;
    }
    for (const TCHAR* ClassName : {TEXT("Warrior"), TEXT("Mage"), TEXT("Archer")})
        bValid &= Find<UWidget>(Pages[3], FString(TEXT("Choose")) + ClassName + TEXT("Selected")) != nullptr;
    if (!bValid) { Shutdown(); return false; }
    for (UEditableTextBox* PasswordInput : {
        Find<UEditableTextBox>(Pages[0], TEXT("Password")),
        Find<UEditableTextBox>(Pages[1], TEXT("Password")),
        Find<UEditableTextBox>(Pages[1], TEXT("ConfirmPassword"))})
    {
        PasswordInput->SetIsPassword(true);
        PasswordInput->OnTextChanged.AddUniqueDynamic(this, &ThisClass::FilterPasswordText);
    }
    for (FName ClassId : {FName(TEXT("Warrior")), FName(TEXT("Mage")), FName(TEXT("Archer"))})
        Portrait(Pages[3], ClassId.ToString() + TEXT("Portrait"), ClassId, true);
    ReplySerial = PC->GetAccountReplySerial(); bLoggedIn = PC->IsLoggedIn();
    Show(bLoggedIn ? 2 : 0); SelectClass(TEXT("Warrior")); Refresh(); return true;
}

void UTDAccountScreenPresenter::Shutdown()
{
    for (UUserWidget* Widget : Pages)
    {
        if (!Widget) continue;
        TArray<UWidget*> Children; Widget->WidgetTree->GetAllWidgets(Children);
        for (auto* Child : Children)
        {
            if (auto* Button = Cast<UButton>(Child)) Button->OnClicked.RemoveAll(this);
            if (auto* TextBox = Cast<UEditableTextBox>(Child)) TextBox->OnTextChanged.RemoveAll(this);
        }
        // WBP가 소유한 화면은 제거하지 않고 이벤트 연결만 해제한다.
    }
    Pages.Reset(); Host.Reset();
}

void UTDAccountScreenPresenter::Show(int32 Index)
{
    Page = Index;
    if (Page != 2) { bDeleteOpen=false; DeleteTarget.Invalidate(); }
    for (int32 I=0; I<Pages.Num(); ++I) Pages[I]->SetVisibility(I==Page ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UTDAccountScreenPresenter::Refresh()
{
    auto* PC = Controller(); auto* State = PC ? PC->GetPlayerState<ATDPlayerState>() : nullptr;
    if (!State || Pages.Num()!=4) return;
    if (ReplySerial != PC->GetAccountReplySerial())
    {
        ReplySerial = PC->GetAccountReplySerial(); bPending = false; Message = FText::FromString(PC->GetAccountMessage());
        if (PC->GetAccountReplyAction()==PendingAction && PC->WasAccountActionSuccessful())
        {
            if (PendingAction==TEXT("Register")) Show(0);
            if (PendingAction==TEXT("Create")) { SelectedSlot=INDEX_NONE; RosterKey.Empty(); Show(2); }
            if (PendingAction==TEXT("Delete")) { bDeleteOpen=false; DeleteTarget.Invalidate(); SelectedSlot=INDEX_NONE; RosterKey.Empty(); }
        }
        if (PendingAction==TEXT("Delete") && bDeleteOpen)
            Text(Pages[2],TEXT("DeleteMessage"),FText::Format(FText::FromString(TEXT("'{0}'\n{1}")),FText::FromString(DeleteTargetName),Message));
        PendingAction=NAME_None;
    }
    if (bLoggedIn != PC->IsLoggedIn())
    { bLoggedIn=PC->IsLoggedIn(); bPending=false; SelectedSlot=INDEX_NONE; RosterKey.Empty(); Show(bLoggedIn?2:0); }
    if (Page==2) RenderRoster();
    Text(Pages[0],TEXT("Status"),Message); Text(Pages[1],TEXT("Status"),Message);
    Text(Pages[2],TEXT("Description"),Message); Text(Pages[3],TEXT("NameStatus"),Message);
    for (int32 P=0;P<Pages.Num();++P)
    {
        TArray<UWidget*> Children; Pages[P]->WidgetTree->GetAllWidgets(Children);
        for (auto* Child:Children) if (auto* Button=Cast<UButton>(Child)) Button->SetIsEnabled(!bPending && !bDeleteOpen);
    }
    Find<UButton>(Pages[2],TEXT("StartGame"))->SetIsEnabled(!bPending && !bDeleteOpen && State->GetCharacterSlots().IsValidIndex(SelectedSlot) && !State->HasSelectedCharacter());
    Find<UButton>(Pages[2],TEXT("OpenCreate"))->SetIsEnabled(!bPending && !bDeleteOpen && State->GetCharacterSlots().Num()<6);
    Find<UButton>(Pages[2],TEXT("OpenDelete"))->SetIsEnabled(!bPending && !bDeleteOpen && State->GetCharacterSlots().IsValidIndex(SelectedSlot));
    Find<UCanvasPanel>(Pages[2],TEXT("DeletePopup"))->SetVisibility(bDeleteOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    Find<UButton>(Pages[2],TEXT("ConfirmDelete"))->SetIsEnabled(bDeleteOpen && !bPending && DeleteTarget.IsValid());
    Find<UButton>(Pages[2],TEXT("CancelDelete"))->SetIsEnabled(bDeleteOpen && !bPending);
}

void UTDAccountScreenPresenter::RenderRoster()
{
    auto* State=Controller()->GetPlayerState<ATDPlayerState>(); const auto& Slots=State->GetCharacterSlots();
    FString Key=FString::FromInt(SelectedSlot);
    for (const auto& S:Slots) Key+=FString::Printf(TEXT("|%s:%s:%d"),*S.CharacterName,*S.ClassId.ToString(),S.Level) + S.CharacterId.ToString();
    if (Key==RosterKey) return; RosterKey=Key;
    for (int32 I=0;I<6;++I)
    {
        const FString Prefix=FString::Printf(TEXT("Slot%d"),I);
        const bool bExists=Slots.IsValidIndex(I);
        Portrait(Pages[2],Prefix+TEXT("FullBody"),bExists?Slots[I].ClassId:NAME_None);
        Text(Pages[2],Prefix+TEXT("Label"),bExists?FText::FromString(FString::Printf(TEXT("Lv.%d  %s"),Slots[I].Level,*Slots[I].CharacterName)):FText::FromString(TEXT("＋ 새 캐릭터")));
        Find<UWidget>(Pages[2],Prefix+TEXT("ButtonSelected"))->SetVisibility(I==SelectedSlot ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    Text(Pages[2],TEXT("RosterCount"),FText::FromString(FString::Printf(TEXT("%d / 6"),Slots.Num())));
    if (Slots.IsValidIndex(SelectedSlot))
    {
        const auto& S=Slots[SelectedSlot]; const auto* Row=ClassRow(S.ClassId);
        Text(Pages[2],TEXT("CharacterName"),FText::FromString(S.CharacterName));
        Text(Pages[2],TEXT("CharacterLevel"),FText::Format(FText::FromString(TEXT("Lv.{0} · {1}")),FText::AsNumber(S.Level),Row?Row->DisplayName:FText::FromName(S.ClassId)));
        Portrait(Pages[2],TEXT("SelectedFullBody"),S.ClassId);
        Portrait(Pages[2],TEXT("SelectedClassPortrait"),S.ClassId,true);
        Text(Pages[2],TEXT("SelectedClassCaption"),Row?Row->DisplayName:FText::FromName(S.ClassId));
    }
    else
    {
        Portrait(Pages[2],TEXT("SelectedFullBody"),NAME_None);
        Portrait(Pages[2],TEXT("SelectedClassPortrait"),NAME_None,true);
        Text(Pages[2],TEXT("CharacterName"),FText::FromString(TEXT("캐릭터를 선택해 주세요")));
        Text(Pages[2],TEXT("CharacterLevel"),FText::GetEmpty()); Text(Pages[2],TEXT("SelectedClassCaption"),FText::GetEmpty());
    }
    Text(Pages[2],TEXT("Location"),FText::GetEmpty());
}

void UTDAccountScreenPresenter::Login()
{
    if (bPending || bDeleteOpen || !Controller()) return;
    bPending=true; Message=FText::FromString(TEXT("로그인 중..."));
    auto* Password=Find<UEditableTextBox>(Pages[0],TEXT("Password"));
    Controller()->ServerLogin(Find<UEditableTextBox>(Pages[0],TEXT("LoginId"))->GetText().ToString().TrimStartAndEnd(),Password->GetText().ToString()); Password->SetText(FText::GetEmpty()); Refresh();
}
void UTDAccountScreenPresenter::FilterPasswordText(const FText& Text)
{
    if (bFilteringPasswordText || Pages.Num() != 4) return;
    TGuardValue<bool> FilteringGuard(bFilteringPasswordText, true);
    for (UEditableTextBox* PasswordInput : {
        Find<UEditableTextBox>(Pages[0], TEXT("Password")),
        Find<UEditableTextBox>(Pages[1], TEXT("Password")),
        Find<UEditableTextBox>(Pages[1], TEXT("ConfirmPassword"))})
    {
        if (!PasswordInput) continue;
        const FString Current = PasswordInput->GetText().ToString();
        const FString Filtered = UTDAccountSubSystem::FilterPasswordInput(Current);
        if (Current != Filtered) PasswordInput->SetText(FText::FromString(Filtered));
    }
}
void UTDAccountScreenPresenter::Register()
{
    if (bPending || bDeleteOpen || !Controller()) return;
    auto* Password=Find<UEditableTextBox>(Pages[1],TEXT("Password"));auto* Confirm=Find<UEditableTextBox>(Pages[1],TEXT("ConfirmPassword"));
    if (!Password->GetText().EqualTo(Confirm->GetText())) { Message=FText::FromString(TEXT("비밀번호가 일치하지 않습니다.")); Refresh(); return; }
    if (!UTDAccountSubSystem::IsValidPassword(Password->GetText().ToString(), true)) { Message=FText::FromString(TEXT("비밀번호는 공백 없이 영문, 숫자, 특수문자를 사용해 8~64자로 입력해 주세요.")); Refresh(); return; }
    auto* Id=Find<UEditableTextBox>(Pages[1],TEXT("LoginId"));Find<UEditableTextBox>(Pages[0],TEXT("LoginId"))->SetText(Id->GetText());
    bPending=true;PendingAction=TEXT("Register");Message=FText::FromString(TEXT("가입 처리 중..."));
    Controller()->ServerRegisterAccount(Id->GetText().ToString().TrimStartAndEnd(),Password->GetText().ToString());Password->SetText(FText::GetEmpty());Confirm->SetText(FText::GetEmpty());Refresh();
}
void UTDAccountScreenPresenter::OpenRegister() { if (!bPending) { Message=FText::GetEmpty();Show(1);Refresh(); } }
void UTDAccountScreenPresenter::BackToLogin() { if (!bPending) { Find<UEditableTextBox>(Pages[1],TEXT("Password"))->SetText(FText::GetEmpty());Find<UEditableTextBox>(Pages[1],TEXT("ConfirmPassword"))->SetText(FText::GetEmpty());Message=FText::GetEmpty();Show(0);Refresh(); } }
void UTDAccountScreenPresenter::OpenCreate()
{
    if (bPending || bDeleteOpen || !Controller()) return;
    auto* S=Controller()->GetPlayerState<ATDPlayerState>();if (!S || S->GetCharacterSlots().Num()>=6) return;
    Message=FText::FromString(TEXT("이름과 직업을 선택해 주세요."));Show(3);Refresh();
}
void UTDAccountScreenPresenter::BackToSelect() { if (!bPending) { Message=FText::GetEmpty();Show(2);Refresh(); } }
void UTDAccountScreenPresenter::CreateCharacter()
{
    if (bPending || bDeleteOpen || !Controller()) return;
    bPending=true;PendingAction=TEXT("Create");Message=FText::FromString(TEXT("캐릭터 생성 중..."));
    Controller()->ServerCreateCharacter(Find<UEditableTextBox>(Pages[3],TEXT("CharacterName"))->GetText().ToString().TrimStartAndEnd(),SelectedClass);Refresh();
}
void UTDAccountScreenPresenter::StartGame()
{
    if (bPending || bDeleteOpen || !Controller() || SelectedSlot==INDEX_NONE) return;
    bPending=true;Message=FText::FromString(TEXT("월드에 입장하는 중..."));Controller()->ServerSelectCharacter(SelectedSlot);Refresh();
}
void UTDAccountScreenPresenter::Logout() { if (!bPending && !bDeleteOpen && Controller()) { bPending=true;Controller()->TDLogout();Refresh(); } }
void UTDAccountScreenPresenter::SelectSlot(int32 Index)
{
    if (bPending || bDeleteOpen || !Controller()) return;
    auto* S=Controller()->GetPlayerState<ATDPlayerState>();if (!S) return;
    if (!S->GetCharacterSlots().IsValidIndex(Index)) { OpenCreate();return; }
    SelectedSlot=Index;Message=FText::GetEmpty();Refresh();
}
void UTDAccountScreenPresenter::SelectClass(FName Id)
{
    if (bPending || !ClassRow(Id)) return;
    SelectedClass=Id;Portrait(Pages[3],TEXT("SelectedFullBody"),Id);Text(Pages[3],TEXT("SelectedClassCaption"),ClassRow(Id)->DisplayName);
    for (FName C : {FName(TEXT("Warrior")),FName(TEXT("Mage")),FName(TEXT("Archer"))})
        Find<UWidget>(Pages[3],TEXT("Choose")+C.ToString()+TEXT("Selected"))->SetVisibility(C==Id ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
void UTDAccountScreenPresenter::Warrior(){SelectClass(TEXT("Warrior"));}
void UTDAccountScreenPresenter::Mage(){SelectClass(TEXT("Mage"));}
void UTDAccountScreenPresenter::Archer(){SelectClass(TEXT("Archer"));}
void UTDAccountScreenPresenter::Slot0(){SelectSlot(0);}
void UTDAccountScreenPresenter::Slot1(){SelectSlot(1);}
void UTDAccountScreenPresenter::Slot2(){SelectSlot(2);}
void UTDAccountScreenPresenter::Slot3(){SelectSlot(3);}
void UTDAccountScreenPresenter::Slot4(){SelectSlot(4);}
void UTDAccountScreenPresenter::Slot5(){SelectSlot(5);}

void UTDAccountScreenPresenter::OpenDelete()
{
    if (bPending || bDeleteOpen || Page!=2 || !Controller()) return;
    const auto* State=Controller()->GetPlayerState<ATDPlayerState>();
    if (!State || !State->GetCharacterSlots().IsValidIndex(SelectedSlot)) return;
    const auto& Target=State->GetCharacterSlots()[SelectedSlot];
    if (!Target.CharacterId.IsValid()) { Message=FText::FromString(TEXT("목록을 다시 불러온 뒤 시도해 주세요."));Refresh();return; }
    DeleteTarget=Target.CharacterId; DeleteTargetName=Target.CharacterName; bDeleteOpen=true;
    Text(Pages[2],TEXT("DeleteMessage"),FText::Format(FText::FromString(TEXT("'{0}' 캐릭터를 삭제하시겠습니까?\n장비와 퀘스트 진행을 포함한 캐릭터 데이터가 삭제됩니다.")),FText::FromString(DeleteTargetName)));
    Refresh(); Find<UButton>(Pages[2],TEXT("CancelDelete"))->SetKeyboardFocus();
}
void UTDAccountScreenPresenter::ConfirmDelete()
{
    if (!bDeleteOpen || bPending || !DeleteTarget.IsValid() || !Controller()) return;
    bPending=true; PendingAction=TEXT("Delete"); Message=FText::FromString(TEXT("캐릭터 삭제 중..."));
    Controller()->ServerDeleteCharacter(DeleteTarget); Refresh();
}
void UTDAccountScreenPresenter::CancelDelete()
{
    if (bPending) return;
    bDeleteOpen=false; DeleteTarget.Invalidate(); DeleteTargetName.Empty(); Refresh();
    Find<UButton>(Pages[2],TEXT("OpenDelete"))->SetKeyboardFocus();
}
