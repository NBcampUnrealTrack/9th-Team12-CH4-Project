#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FieldNotificationId.h"
#include "UI/HUD/TDProgressBarAnimation.h"
#include "TDEXPBarWidget.generated.h"

class UTDPlayerStatsViewModel;
class UProgressBar;
class UTextBlock;
class UTDProgressBarStyleDA;

/** 플레이어 HUD와 같은 뷰모델을 사용하되 EXP 필드만 구독한다. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDEXPBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="TD|UI|EXP")
		void SetViewModel(UTDPlayerStatsViewModel* InViewModel);
	UFUNCTION(BlueprintPure, Category="TD|UI|EXP")
		UTDPlayerStatsViewModel* GetViewModel() const { return ViewModel; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="TD|UI|EXP")
		TObjectPtr<UProgressBar> ExpBar;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="TD|UI|EXP")
		TObjectPtr<UTextBlock> ExpPercentText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="TD|UI|EXP")
		TObjectPtr<UProgressBar> ExpFlashBar;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|EXP Effects")
		bool bAnimateExperience = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|EXP Effects")
		TObjectPtr<UTDProgressBarStyleDA> ExpBarStyleData;

private:
	UPROPERTY(Transient)
		TObjectPtr<UTDPlayerStatsViewModel> ViewModel;
	FTDProgressBarAnimation Animation;
	bool bProgressPending = false;
	void UnbindViewModel();
	void OnFieldChanged(UObject* Object, UE::FieldNotification::FFieldId Field);
	void RefreshProgress(bool bAnimate);
	void ApplyVisuals();
};
