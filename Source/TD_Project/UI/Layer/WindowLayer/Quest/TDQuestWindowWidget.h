#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UI/Layer/WindowLayer/Common/WindowBase/TDWindowBaseWidget.h"
#include "TDQuestWindowWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UTDQuestComponent;

/**
 * 메인 1개 + 서브 최대 2개를 한 화면에 표시하는 퀘스트 창.
 * 서브 퀘스트만 확인창을 거쳐 포기할 수 있습니다.
 */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDQuestWindowWidget
    : public UTDWindowBaseWidget
{
    GENERATED_BODY()

public:
    /**
     * 별도 경고창에서 포기 또는 취소를 선택했을 때 호출합니다.
     * 확인창을 열었던 퀘스트와 같은 ID인지 검사합니다.
     */
    UFUNCTION(BlueprintCallable, Category = "TD|QuestWindow")
    void ResolveAbandonConfirmation(
        FName InQuestId,
        bool bConfirmed);

    virtual void CloseWindow_Implementation() override;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // 메인 퀘스트 영역
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> ROW_Main;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_MainTitle;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_MainDescription;

    /** WBP에서 라벨 옆에 배치할 메인 퀘스트 아이템 값입니다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_MainRewardItem;

    /** WBP에서 라벨 옆에 배치할 메인 퀘스트 경험치 값입니다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_MainRewardExp;

    /** WBP에서 라벨 옆에 배치할 메인 퀘스트 골드 값입니다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_MainRewardGold;

    // 첫 번째 서브 퀘스트 영역
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> ROW_Sub1;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Sub1Title;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Sub1Description;

    /** WBP에서 라벨 옆에 배치할 첫 번째 서브 퀘스트 아이템 값입니다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Sub1RewardItem;

    /** WBP에서 라벨 옆에 배치할 첫 번째 서브 퀘스트 경험치 값입니다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Sub1RewardExp;

    /** WBP에서 라벨 옆에 배치할 첫 번째 서브 퀘스트 골드 값입니다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Sub1RewardGold;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Sub1Abandon;

    // 두 번째 서브 퀘스트 영역
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> ROW_Sub2;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Sub2Title;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Sub2Description;

    /** WBP에서 라벨 옆에 배치할 두 번째 서브 퀘스트 아이템 값입니다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Sub2RewardItem;

    /** WBP에서 라벨 옆에 배치할 두 번째 서브 퀘스트 경험치 값입니다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Sub2RewardExp;

    /** WBP에서 라벨 옆에 배치할 두 번째 서브 퀘스트 골드 값입니다. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Sub2RewardGold;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Sub2Abandon;
    

    // 목록 없음 / 처리 중 등의 안내 문구
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Message;

    /**
     * 블루프린트에서 별도 경고창을 생성합니다.
     * 화면 표시까지 성공하면 true를 반환합니다.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "TD|QuestWindow")
    bool ShowAbandonPopup(
        FName InQuestId,
        const FText& InQuestTitle);

    /** 블루프린트에서 만들어 둔 경고창을 닫습니다. */
    UFUNCTION(BlueprintImplementableEvent, Category = "TD|QuestWindow")
    void HideAbandonPopup();
    
private:
    TWeakObjectPtr<UTDQuestComponent> BoundQuestComponent;

    FTimerHandle BindRetryTimer;
    FTimerHandle AbandonWaitTimer;

    FName SubQuest1Id = NAME_None;
    FName SubQuest2Id = NAME_None;

    // 확인창을 열 때 선택한 퀘스트입니다.
    FName ConfirmQuestId = NAME_None;

    // 서버에 포기를 요청한 뒤 결과를 기다리는 퀘스트입니다.
    FName PendingAbandonQuestId = NAME_None;

    bool bWidgetsReady = false;

    void TryBindQuestComponent();
    void OpenAbandonConfirmation(FName InQuestId);
    void CloseAbandonConfirmation();
    void SetMessage(const FText& InText);
    void HandleAbandonWaitExpired();

    UFUNCTION()
    void RefreshQuestWindow();

    UFUNCTION()
    void HandleSub1Abandon();

    UFUNCTION()
    void HandleSub2Abandon();

    UFUNCTION()
    void HandleConfirmYes();

    UFUNCTION()
    void HandleConfirmNo();
    
};
