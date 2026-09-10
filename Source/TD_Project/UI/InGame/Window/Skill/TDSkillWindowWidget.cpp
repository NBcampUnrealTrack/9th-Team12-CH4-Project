#include "UI/InGame/Window/Skill/TDSkillWindowWidget.h"
#include "UI/InGame/Window/Skill/TDSkillEntryWidget.h"
#include "Stats/TDProgressionComponent.h"
#include "Player/TDPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UTDSkillWindowWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshSkills();
    GetWorld()->GetTimerManager().SetTimer(SourceTimer, this, &ThisClass::RefreshSkills, .25f, true);
}

void UTDSkillWindowWidget::NativeDestruct()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(SourceTimer);
    if (Source.IsValid()) Source->OnProgressionChanged.RemoveDynamic(this, &ThisClass::RefreshSkills);
    Source.Reset();
    DisplayedClass = NAME_None;
    Entries.Reset();
    Super::NativeDestruct();
}

void UTDSkillWindowWidget::RefreshSkills()
{
    const APlayerController* Controller = GetOwningPlayer();
    const ATDPlayerState* State = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;
    UTDProgressionComponent* Next = State ? State->GetProgressionComponent() : nullptr;
    const FName NextClass = Next ? Next->GetClassId() : NAME_None;
    const bool bRebuild = Source.Get() != Next || DisplayedClass != NextClass;
    if (Source.Get() != Next)
    {
        if (Source.IsValid()) Source->OnProgressionChanged.RemoveDynamic(this, &ThisClass::RefreshSkills);
        Source = Next;
        if (Next) Next->OnProgressionChanged.AddUniqueDynamic(this, &ThisClass::RefreshSkills);
    }
    DisplayedClass = NextClass;
    if (bRebuild) RebuildEntries();
    if (PointsText) PointsText->SetText(FText::Format(PointsFormat, FText::AsNumber(Next ? Next->GetRemainingSkillPoints() : 0)));
    for (UTDSkillEntryWidget* Entry : Entries) if (Entry) Entry->RefreshSkill();
    if (ActiveEmptyText) ActiveEmptyText->SetVisibility(ActiveList && ActiveList->GetChildrenCount() > 0 ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    if (PassiveEmptyText) PassiveEmptyText->SetVisibility(PassiveList && PassiveList->GetChildrenCount() > 0 ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UTDSkillWindowWidget::RebuildEntries()
{
    Entries.Reset();
    if (ActiveList) ActiveList->ClearChildren();
    if (PassiveList) PassiveList->ClearChildren();
    UTDProgressionComponent* Progression = Source.Get();
    if (!Progression || !EntryWidgetClass || !ActiveList || !PassiveList) return;
    TArray<FName> Skills = Progression->GetClassSkills();
    Skills.Sort([Progression](FName A, FName B)
    {
        const FTDSkillRow* RowA = Progression->FindSkillRow(A);
        const FTDSkillRow* RowB = Progression->FindSkillRow(B);
        if (RowA && RowB && RowA->SlotIndex != RowB->SlotIndex) return RowA->SlotIndex < RowB->SlotIndex;
        return A.LexicalLess(B);
    });
    for (FName Id : Skills)
    {
        const FTDSkillRow* Row = Progression->FindSkillRow(Id);
        if (!Row) continue;
        UTDSkillEntryWidget* Entry = CreateWidget<UTDSkillEntryWidget>(GetOwningPlayer(), EntryWidgetClass);
        if (!Entry) continue;
        (Row->SkillType == ETDSkillType::Active ? ActiveList : PassiveList)->AddChildToVerticalBox(Entry);
        Entry->SetSkill(Progression, Id);
        Entries.Add(Entry);
    }
}
