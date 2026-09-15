#include "TDQuestWindowWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDItemRow.h"
#include "Data/TDQuestTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Items/TDInventoryComponent.h"
#include "Player/TDPlayerState.h"
#include "Quest/TDQuestComponent.h"

#define LOCTEXT_NAMESPACE "TDQuestWindowWidget"

namespace
{
    FText MakeQuestItemRewardValue(
        const FTDQuestViewData& InQuest,
        const UTDInventoryComponent* InInventory)
    {
        TArray<FString> RewardParts;

        for (const FTDQuestItemReward& ItemReward : InQuest.ItemRewards)
        {
            if (ItemReward.ItemId.IsNone() || ItemReward.Count <= 0)
            {
                continue;
            }

            FString ItemName = ItemReward.ItemId.ToString();

            if (InInventory)
            {
                if (const FTDItemRow* ItemDefinition =
                    InInventory->FindItemDefinition(ItemReward.ItemId))
                {
                    if (!ItemDefinition->DisplayName.IsEmpty())
                    {
                        ItemName = ItemDefinition->DisplayName.ToString();
                    }
                }
            }

            RewardParts.Add(FString::Printf(
                TEXT("%s x%s"),
                *ItemName,
                *FText::AsNumber(ItemReward.Count).ToString()));
        }

        return FText::FromString(
            RewardParts.IsEmpty()
                ? TEXT("-")
                : FString::Join(RewardParts, TEXT(" / ")));
    }

    void UpdateQuestRewardValues(
        UTextBlock* InItemReward,
        UTextBlock* InExpReward,
        UTextBlock* InGoldReward,
        const FTDQuestViewData* InQuest,
        const UTDInventoryComponent* InInventory)
    {
        UTextBlock* ValueWidgets[] =
        {
            InItemReward,
            InExpReward,
            InGoldReward
        };

        for (UTextBlock* ValueWidget : ValueWidgets)
        {
            if (ValueWidget)
            {
                ValueWidget->SetVisibility(
                    InQuest
                        ? ESlateVisibility::Visible
                        : ESlateVisibility::Collapsed);
            }
        }

        if (!InQuest)
        {
            return;
        }

        if (InItemReward)
        {
            InItemReward->SetText(
                MakeQuestItemRewardValue(*InQuest, InInventory));
        }

        if (InExpReward)
        {
            InExpReward->SetText(
                FText::AsNumber(InQuest->ExpReward));
        }

        if (InGoldReward)
        {
            InGoldReward->SetText(
                FText::AsNumber(InQuest->GoldReward));
        }
    }

    // 한 퀘스트 영역의 제목과 설명을 갱신합니다.
    // 색상은 변경하지 않고 위젯 디자이너 설정을 그대로 사용합니다.
    void UpdateQuestRow(
        UBorder* InRow,
        UTextBlock* InTitle,
        UTextBlock* InDescription,
        const FTDQuestViewData* InQuest)
    {
        InRow->SetVisibility(
            InQuest
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);

        if (!InQuest)
        {
            InTitle->SetText(FText::GetEmpty());
            InDescription->SetText(FText::GetEmpty());
            return;
        }

        InTitle->SetText(InQuest->DisplayName);
        InDescription->SetText(InQuest->Description);
        InDescription->SetAutoWrapText(true);
    }
}

void UTDQuestWindowWidget::NativeConstruct()
{
    Super::NativeConstruct();

    bWidgetsReady =
        ROW_Main
        && TXT_MainTitle
        && TXT_MainDescription
        && ROW_Sub1
        && TXT_Sub1Title
        && TXT_Sub1Description
        && BTN_Sub1Abandon
        && ROW_Sub2
        && TXT_Sub2Title
        && TXT_Sub2Description
        && BTN_Sub2Abandon
        && TXT_Message;

    if (!ensureMsgf(
        bWidgetsReady,
        TEXT("WBP_QuestWindow: required widget bindings are missing.")))
    {
        return;
    }

    BTN_Sub1Abandon->OnClicked.AddUniqueDynamic(
        this,
        &UTDQuestWindowWidget::HandleSub1Abandon);

    BTN_Sub2Abandon->OnClicked.AddUniqueDynamic(
        this,
        &UTDQuestWindowWidget::HandleSub2Abandon);

    SubQuest1Id = NAME_None;
    SubQuest2Id = NAME_None;
    PendingAbandonQuestId = NAME_None;

    SetWindowTitle(LOCTEXT("WindowTitle", "퀘스트"));
    
    TXT_Message->SetAutoWrapText(true);

    CloseAbandonConfirmation();
    RefreshQuestWindow();
    TryBindQuestComponent();

    // The HUD/window can survive a character switch; keep watching its owner.
    if (UWorld* CurrentWorld = GetWorld())
    {
        CurrentWorld->GetTimerManager().SetTimer(
            BindRetryTimer,
            this,
            &UTDQuestWindowWidget::TryBindQuestComponent,
            0.25f,
            true);
    }
}

void UTDQuestWindowWidget::NativeDestruct()
{
    CloseAbandonConfirmation();

    bWidgetsReady = false;

    if (UWorld* CurrentWorld = GetWorld())
    {
        CurrentWorld->GetTimerManager().ClearTimer(BindRetryTimer);
        CurrentWorld->GetTimerManager().ClearTimer(AbandonWaitTimer);
    }

    if (UTDQuestComponent* QuestComponent = BoundQuestComponent.Get())
    {
        QuestComponent->OnQuestListChanged.RemoveDynamic(
            this,
            &UTDQuestWindowWidget::RefreshQuestWindow);
    }

    UButton* BoundButtons[] =
    {
        BTN_Sub1Abandon.Get(),
        BTN_Sub2Abandon.Get()
    };

    for (UButton* BoundButton : BoundButtons)
    {
        if (BoundButton)
        {
            BoundButton->OnClicked.RemoveAll(this);
        }
    }

    BoundQuestComponent.Reset();

    SubQuest1Id = NAME_None;
    SubQuest2Id = NAME_None;
    ConfirmQuestId = NAME_None;
    PendingAbandonQuestId = NAME_None;

    Super::NativeDestruct();
}

void UTDQuestWindowWidget::TryBindQuestComponent()
{
    if (!bWidgetsReady)
    {
        return;
    }

    ATDPlayerState* TDState =
        GetOwningPlayerState<ATDPlayerState>();

    UTDQuestComponent* QuestComponent = IsValid(TDState) && TDState->HasSelectedCharacter()
        ? TDState->GetQuestComponent() : nullptr;
    if (BoundQuestComponent.Get() == QuestComponent && !BoundQuestComponent.IsStale())
    {
        return;
    }

    if (UTDQuestComponent* Previous = BoundQuestComponent.Get())
        Previous->OnQuestListChanged.RemoveDynamic(this, &UTDQuestWindowWidget::RefreshQuestWindow);
    CloseAbandonConfirmation();
    if (UWorld* CurrentWorld = GetWorld())
        CurrentWorld->GetTimerManager().ClearTimer(AbandonWaitTimer);
    SubQuest1Id = NAME_None;
    SubQuest2Id = NAME_None;
    PendingAbandonQuestId = NAME_None;
    BoundQuestComponent = QuestComponent;
    TXT_Message->SetText(FText::GetEmpty());

    if (QuestComponent)
        QuestComponent->OnQuestListChanged.AddUniqueDynamic(
        this,
        &UTDQuestWindowWidget::RefreshQuestWindow);

    RefreshQuestWindow();
}

void UTDQuestWindowWidget::RefreshQuestWindow()
{
    if (!bWidgetsReady)
    {
        return;
    }

    UTDQuestComponent* QuestComponent = BoundQuestComponent.Get();

    TArray<FTDQuestViewData> QuestViews;

    if (QuestComponent)
    {
        QuestViews = QuestComponent->GetQuestTrackerViews();
    }

    const FTDQuestViewData* MainView = nullptr;
    const FTDQuestViewData* SubViews[2] = { nullptr, nullptr };
    int32 SubCount = 0;

    for (const FTDQuestViewData& QuestView : QuestViews)
    {
        if (QuestView.QuestTypeTag == TDTags::Quest_Type_Main.GetTag())
        {
            if (!MainView)
            {
                MainView = &QuestView;
            }
        }
        else if (
            QuestView.QuestTypeTag == TDTags::Quest_Type_Sub.GetTag()
            && SubCount < 2)
        {
            SubViews[SubCount] = &QuestView;
            ++SubCount;
        }
    }

    SubQuest1Id =
        SubViews[0] ? SubViews[0]->QuestId : NAME_None;

    SubQuest2Id =
        SubViews[1] ? SubViews[1]->QuestId : NAME_None;

    const ATDPlayerState* TDState =
        GetOwningPlayerState<ATDPlayerState>();

    const UTDInventoryComponent* Inventory =
        TDState ? TDState->GetInventoryComponent() : nullptr;

    UpdateQuestRewardValues(
        TXT_MainRewardItem,
        TXT_MainRewardExp,
        TXT_MainRewardGold,
        MainView,
        Inventory);

    UpdateQuestRewardValues(
        TXT_Sub1RewardItem,
        TXT_Sub1RewardExp,
        TXT_Sub1RewardGold,
        SubViews[0],
        Inventory);

    UpdateQuestRewardValues(
        TXT_Sub2RewardItem,
        TXT_Sub2RewardExp,
        TXT_Sub2RewardGold,
        SubViews[1],
        Inventory);

    UpdateQuestRow(
        ROW_Main,
        TXT_MainTitle,
        TXT_MainDescription,
        MainView);

    UpdateQuestRow(
        ROW_Sub1,
        TXT_Sub1Title,
        TXT_Sub1Description,
        SubViews[0]);

    UpdateQuestRow(
        ROW_Sub2,
        TXT_Sub2Title,
        TXT_Sub2Description,
        SubViews[1]);

    // 확인창을 열어 둔 사이에 해당 퀘스트가 없어지면 확인창을 닫습니다.
    if (!ConfirmQuestId.IsNone()
        && ConfirmQuestId != SubQuest1Id
        && ConfirmQuestId != SubQuest2Id)
    {
        CloseAbandonConfirmation();
    }

    // 서버에서 전달된 목록에서 대상이 사라졌다면 대기를 끝냅니다.
    if (QuestComponent
        && !PendingAbandonQuestId.IsNone()
        && PendingAbandonQuestId != SubQuest1Id
        && PendingAbandonQuestId != SubQuest2Id)
    {
        PendingAbandonQuestId = NAME_None;

        if (UWorld* CurrentWorld = GetWorld())
        {
            CurrentWorld->GetTimerManager().ClearTimer(AbandonWaitTimer);
        }
    }

    const bool bCanRequestAbandon =
        QuestComponent != nullptr
        && ConfirmQuestId.IsNone()
        && PendingAbandonQuestId.IsNone();

    BTN_Sub1Abandon->SetIsEnabled(
        bCanRequestAbandon && !SubQuest1Id.IsNone());

    BTN_Sub2Abandon->SetIsEnabled(
        bCanRequestAbandon && !SubQuest2Id.IsNone());

    if (!QuestComponent)
    {
        SetMessage(
            LOCTEXT("Loading", "퀘스트 정보를 불러오는 중입니다."));
    }
    else if (!PendingAbandonQuestId.IsNone())
    {
        SetMessage(
            LOCTEXT("Waiting", "퀘스트 포기 요청을 처리하고 있습니다."));
    }
    else if (!MainView && SubCount == 0)
    {
        SetMessage(
            LOCTEXT("Empty", "진행 중인 퀘스트가 없습니다."));
    }
    else
    {
        SetMessage(FText::GetEmpty());
    }
}

void UTDQuestWindowWidget::HandleSub1Abandon()
{
    OpenAbandonConfirmation(SubQuest1Id);
}

void UTDQuestWindowWidget::HandleSub2Abandon()
{
    OpenAbandonConfirmation(SubQuest2Id);
}

void UTDQuestWindowWidget::OpenAbandonConfirmation(FName InQuestId)
{
    UTDQuestComponent* QuestComponent = BoundQuestComponent.Get();

    if (!bWidgetsReady
        || !QuestComponent
        || InQuestId.IsNone()
        || !ConfirmQuestId.IsNone()
        || !PendingAbandonQuestId.IsNone())
    {
        return;
    }

    const TArray<FTDQuestViewData> CurrentViews =
        QuestComponent->GetQuestTrackerViews();

    const FTDQuestViewData* SelectedView =
        CurrentViews.FindByPredicate(
            [InQuestId](const FTDQuestViewData& QuestView)
            {
                return QuestView.QuestId == InQuestId
                    && QuestView.QuestTypeTag
                        == TDTags::Quest_Type_Sub.GetTag();
            });

    if (!SelectedView)
    {
        RefreshQuestWindow();
        return;
    }

    // 선택한 퀘스트를 기억하고, 목록의 포기 버튼을 잠시 비활성화합니다.
    ConfirmQuestId = SelectedView->QuestId;
    RefreshQuestWindow();

    // 실제 경고창 생성은 WBP_QuestWindow의 블루프린트가 담당합니다.
    const bool bPopupShown = ShowAbandonPopup(
        SelectedView->QuestId,
        SelectedView->DisplayName);

    if (!bPopupShown)
    {
        CloseAbandonConfirmation();
        RefreshQuestWindow();

        SetMessage(
            LOCTEXT(
                "PopupOpenFailed",
                "포기 확인창을 열지 못했습니다. 위젯 연결을 확인해 주세요."));
    }
}

void UTDQuestWindowWidget::HandleConfirmYes()
{
    if (!bWidgetsReady
        || ConfirmQuestId.IsNone()
        || !PendingAbandonQuestId.IsNone())
    {
        return;
    }

    const FName QuestIdToAbandon = ConfirmQuestId;

    UTDQuestComponent* QuestComponent = BoundQuestComponent.Get();
    UWorld* CurrentWorld = GetWorld();

    if (!QuestComponent || !CurrentWorld)
    {
        CloseAbandonConfirmation();
        RefreshQuestWindow();
        return;
    }

    const TArray<FTDQuestViewData> CurrentViews =
        QuestComponent->GetQuestTrackerViews();

    const bool bStillActiveSubQuest =
        CurrentViews.ContainsByPredicate(
            [QuestIdToAbandon](const FTDQuestViewData& QuestView)
            {
                return QuestView.QuestId == QuestIdToAbandon
                    && QuestView.QuestTypeTag
                        == TDTags::Quest_Type_Sub.GetTag();
            });

    CloseAbandonConfirmation();

    if (!bStillActiveSubQuest)
    {
        RefreshQuestWindow();
        return;
    }

    PendingAbandonQuestId = QuestIdToAbandon;
    RefreshQuestWindow();

    // 서버 목록 변경이 오지 않아도 버튼이 영원히 잠기지 않게 합니다.
    // 리슨 서버에서는 요청 즉시 처리될 수 있어 타이머를 먼저 설정합니다.
    CurrentWorld->GetTimerManager().SetTimer(
        AbandonWaitTimer,
        this,
        &UTDQuestWindowWidget::HandleAbandonWaitExpired,
        8.0f,
        false);

    QuestComponent->ServerAbandonQuest(QuestIdToAbandon);
}

void UTDQuestWindowWidget::HandleConfirmNo()
{
    CloseAbandonConfirmation();
    RefreshQuestWindow();
}

void UTDQuestWindowWidget::CloseAbandonConfirmation()
{
    ConfirmQuestId = NAME_None;
    HideAbandonPopup();
}

void UTDQuestWindowWidget::SetMessage(const FText& InText)
{
    if (!TXT_Message)
    {
        return;
    }

    TXT_Message->SetText(InText);
    TXT_Message->SetVisibility(
        InText.IsEmpty()
            ? ESlateVisibility::Collapsed
            : ESlateVisibility::Visible);
}

void UTDQuestWindowWidget::HandleAbandonWaitExpired()
{
    RefreshQuestWindow();

    if (PendingAbandonQuestId.IsNone())
    {
        return;
    }

    PendingAbandonQuestId = NAME_None;
    RefreshQuestWindow();

    SetMessage(
        LOCTEXT(
            "AbandonWaitExpired",
            "아직 목록 변경을 확인하지 못했습니다. 연결 상태와 퀘스트 목록을 확인해 주세요."));
}

void UTDQuestWindowWidget::ResolveAbandonConfirmation(
    FName InQuestId,
    bool bConfirmed)
{
    // 닫힌 창 또는 이전 확인창에서 온 응답은 처리하지 않습니다.
    if (!bWidgetsReady
        || InQuestId.IsNone()
        || InQuestId != ConfirmQuestId)
    {
        return;
    }

    if (bConfirmed)
    {
        HandleConfirmYes();
    }
    else
    {
        HandleConfirmNo();
    }
}

void UTDQuestWindowWidget::CloseWindow_Implementation()
{
    CloseAbandonConfirmation();
    Super::CloseWindow_Implementation();
}

#undef LOCTEXT_NAMESPACE
