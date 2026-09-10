#include "UI/InGame/Window/Skill/TDSkillEntryWidget.h"
#include "Stats/TDProgressionComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "UI/Common/Tooltip/TDItemTooltipWidget.h"

void UTDSkillEntryWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (UpgradeButton) UpgradeButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestUpgrade);
    RefreshSkill();
}

void UTDSkillEntryWidget::SetSkill(UTDProgressionComponent* InSource, FName InSkillId)
{
    TooltipLevel = INDEX_NONE;
    Source = InSource;
    SkillId = InSkillId;
    RefreshSkill();
}

void UTDSkillEntryWidget::RefreshSkill()
{
    UTDProgressionComponent* Progression = Source.Get();
    const FTDSkillRow* Row = Progression ? Progression->FindSkillRow(SkillId) : nullptr;
    if (!Row)
    {
        SetToolTip(nullptr);
        SetToolTipText(FText::GetEmpty());
        TooltipLevel = INDEX_NONE;
        if (UpgradeButton) UpgradeButton->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    const int32 Level = Progression->GetSkillLevel(SkillId);
    const bool bMax = Level >= Row->MaxLevel;
    SkillNameText->SetText(Row->DisplayName);
    SkillLevelText->SetText(bMax ? MaxLabel : FText::Format(LevelFormat, FText::AsNumber(Level), FText::AsNumber(Row->MaxLevel)));
    SkillInfoText->SetText(FText::Format(RequirementFormat, FText::AsNumber(Row->RequiredLevel)));
    const TCHAR* Keys[] = {TEXT("Q"), TEXT("W"), TEXT("E")};
    const bool bActive = Row->SkillType == ETDSkillType::Active;
    const FString Key = bActive && Row->SlotIndex >= 1 && Row->SlotIndex <= 3 ? Keys[Row->SlotIndex - 1] : TEXT("");
    SkillTypeText->SetText(bActive ? FText::Format(ActiveFormat, FText::FromString(Key)) : PassiveLabel);
    IconFallback->SetText(bActive ? FText::FromString(Key) : PassiveLabel);
    IconFallback->SetVisibility(Row->Icon.IsNull() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (Row->Icon.IsNull()) { SkillIcon->SetBrushFromTexture(nullptr); SkillIcon->SetVisibility(ESlateVisibility::Collapsed); }
    else { SkillIcon->SetBrushFromSoftTexture(Row->Icon); SkillIcon->SetVisibility(ESlateVisibility::HitTestInvisible); }
    // 행 전체에서 공용 툴팁을 보여준다. 강화 버튼은 자체 도움말을 유지한다.
    SetVisibility(ESlateVisibility::Visible);
    if (TooltipLevel != Level)
    {
        FText Details = FText::Format(
            NSLOCTEXT("TDSkillWindow", "SkillTooltip", "{0}\n스킬 레벨: {1} / {2}\n요구 레벨: {3}"),
            SkillTypeText->GetText(), FText::AsNumber(Level),
            FText::AsNumber(Row->MaxLevel), FText::AsNumber(Row->RequiredLevel));
        if (bActive)
        {
            Details = FText::Format(
                NSLOCTEXT("TDSkillWindow", "ActiveTooltip", "{0}\n기본 마나: {1}\n기본 재사용 대기시간: {2}초"),
                Details, FText::AsNumber(Row->ManaCost), FText::AsNumber(Row->Cooldown));
        }
        UTDItemTooltipWidget::AttachText(this, this, Row->DisplayName, Details);
        TooltipLevel = Level;
    }
    // 잔여 포인트가 없을 때는 버튼 자리도 접는다. 요청의 최종 검증은 서버에서 한다.
    UpgradeButton->SetVisibility(Progression->GetRemainingSkillPoints() > 0 && !bMax ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    UpgradeButton->SetIsEnabled(Progression->CanUpgradeSkill(SkillId));
}

void UTDSkillEntryWidget::RequestUpgrade()
{
    if (UTDProgressionComponent* Progression = Source.Get(); Progression && Progression->CanUpgradeSkill(SkillId))
        Progression->ServerUpgradeSkill(SkillId);
}
