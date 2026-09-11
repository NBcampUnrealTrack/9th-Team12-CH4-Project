#include "UI/HUD/TDPartyMemberWidget.h"
#include "UI/ViewModel/TDPlayerStatsViewModel.h"
#include "Player/TDPlayerState.h"
#include "Party/TDPartyComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

void UTDPartyMemberWidget::NativeConstruct()
{
    Super::NativeConstruct();
    ConnectViewModel();
}

void UTDPartyMemberWidget::SetMember(ATDPlayerState* InMember)
{
    if (Member.Get() == InMember && ViewModel) return;
    Member = InMember;
    ConnectViewModel();
}

void UTDPartyMemberWidget::ConnectViewModel()
{
    // 로컬 플레이어 공유 VM을 쓰지 않는다. 파티원마다 별도로 구독한다.
    if (!ViewModel) ViewModel = NewObject<UTDPlayerStatsViewModel>(this);
    ViewModel->RemoveAllFieldValueChangedDelegates(this);
    ViewModel->SetSource(Member.Get());
    using F = UTDPlayerStatsViewModel::FFieldNotificationClassDescriptor;
    const TArray<UE::FieldNotification::FFieldId> Fields = {F::HealthPercent, F::HealthText, F::LevelNameText,
        F::CharacterPortrait, F::CharacterClassIcon, F::CharacterClassName};
    for (const auto Field : Fields)
    {
        ViewModel->AddFieldValueChangedDelegate(Field,
            INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::HandleFieldChanged));
    }
    ApplyDisplay();
}

void UTDPartyMemberWidget::RefreshMember()
{
    // 이름/리더 복제 알림이 UI 연결보다 먼저 도착한 경우도 보완한다.
    if (ViewModel) ViewModel->RefreshAll();
    ApplyDisplay();
}

void UTDPartyMemberWidget::HandleFieldChanged(UObject*, UE::FieldNotification::FFieldId)
{
    ApplyDisplay();
}

void UTDPartyMemberWidget::ApplyDisplay()
{
    if (!ViewModel) return;
    if (LevelNameText) LevelNameText->SetText(ViewModel->LevelNameText);
    if (HealthValueText) HealthValueText->SetText(ViewModel->HealthText);
    if (HealthBar) HealthBar->SetPercent(ViewModel->HealthPercent);
    if (Portrait)
    {
        Portrait->SetBrushFromTexture(ViewModel->CharacterPortrait);
        Portrait->SetVisibility(ViewModel->CharacterPortrait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    if (ClassIcon)
    {
        ClassIcon->SetBrushFromTexture(ViewModel->CharacterClassIcon);
        ClassIcon->SetVisibility(ViewModel->CharacterClassIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    if (ClassFallbackText)
    {
        ClassFallbackText->SetText(ViewModel->CharacterClassName);
        ClassFallbackText->SetVisibility(ViewModel->CharacterPortrait ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
    const ATDPlayerState* State = Member.Get();
    const UTDPartyComponent* Party = State ? State->GetPartyComponent() : nullptr;
    if (LeaderIcon) LeaderIcon->SetVisibility(Party && Party->IsPartyLeader()
        ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void UTDPartyMemberWidget::NativeDestruct()
{
    if (ViewModel)
    {
        ViewModel->RemoveAllFieldValueChangedDelegates(this);
        ViewModel->SetSource(nullptr);
    }
    Super::NativeDestruct();
}
