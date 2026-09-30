#pragma once
#include "CoreMinimal.h"
#include "UI/Layer/WindowLayer/Common/WindowBase/TDWindowBaseWidget.h"
#include "TDSkillWindowWidget.generated.h"

class UTDProgressionComponent;
class UTDSkillEntryWidget;
class UVerticalBox;
class UTextBlock;

/** 목록 배치는 WBP, 직업별 조회와 서버 강화 요청은 기존 Progression을 사용한다. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDSkillWindowWidget : public UTDWindowBaseWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|Skill")
    TSubclassOf<UTDSkillEntryWidget> EntryWidgetClass;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UVerticalBox> ActiveList;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UVerticalBox> PassiveList;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> PointsText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> ActiveEmptyText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> PassiveEmptyText;
    UPROPERTY(EditDefaultsOnly, Category="TD|Skill|Text") FText PointsFormat;
private:
    UFUNCTION() void RefreshSkills();
    void RebuildEntries();
    TWeakObjectPtr<UTDProgressionComponent> Source;
    FName DisplayedClass;
    FTimerHandle SourceTimer;
    UPROPERTY(Transient) TArray<TObjectPtr<UTDSkillEntryWidget>> Entries;
};
