#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDSkillEntryWidget.generated.h"

class UTDProgressionComponent;
class UButton;
class UTextBlock;
class UImage;

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDSkillEntryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetSkill(UTDProgressionComponent* InSource, FName InSkillId);
    void RefreshSkill();
    UFUNCTION(BlueprintCallable, Category="TD|Skill") void RequestUpgrade();
protected:
    virtual void NativeConstruct() override;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> SkillNameText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> SkillLevelText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> SkillInfoText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> SkillTypeText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UImage> SkillIcon;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> IconFallback;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> UpgradeButton;
    UPROPERTY(EditDefaultsOnly, Category="TD|Skill|Text") FText LevelFormat;
    UPROPERTY(EditDefaultsOnly, Category="TD|Skill|Text") FText RequirementFormat;
    UPROPERTY(EditDefaultsOnly, Category="TD|Skill|Text") FText ActiveFormat;
    UPROPERTY(EditDefaultsOnly, Category="TD|Skill|Text") FText PassiveLabel;
    UPROPERTY(EditDefaultsOnly, Category="TD|Skill|Text") FText MaxLabel;
private:
    TWeakObjectPtr<UTDProgressionComponent> Source;
    FName SkillId;
    int32 TooltipLevel = INDEX_NONE;
    int32 TooltipCharacterLevel = INDEX_NONE;
};
