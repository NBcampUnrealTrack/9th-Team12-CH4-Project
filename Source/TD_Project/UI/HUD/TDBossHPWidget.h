#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDBossHPWidget.generated.h"

class UTDBossViewModel;
class UProgressBar;
class UTDTextBlock;

/** 보스 데이터의 표시만 담당한다. 대상 선택, 구독, 표시/숨김은 외부에서 관리한다. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDBossHPWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="TD|UI|Boss HP")
		void SetViewModel(UTDBossViewModel* InViewModel);
	/** 보스가 바뀔 때 이름, 레벨, 체력을 함께 설정한다. */
	UFUNCTION(BlueprintCallable, Category="TD|UI|Boss HP")
		void SetBossDisplay(const FText& InBossName, int32 InBossLevel, float InHealth,
		                    float InMaxHealth);

	/** 추후 ViewModel에서 전달받은 체력을 표시한다. */
	UFUNCTION(BlueprintCallable, Category="TD|UI|Boss HP")
		void SetHealth(float InHealth, float InMaxHealth);

	/** 이전 보스의 표시 데이터를 지운다. 위젯의 Visibility는 변경하지 않는다. */
	UFUNCTION(BlueprintCallable, Category="TD|UI|Boss HP")
		void ClearBossDisplay();

	UFUNCTION(BlueprintPure, Category="TD|UI|Boss HP")
		float GetHealthPercent() const;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(Transient)
		TObjectPtr<UTDBossViewModel> ViewModel;

	/** 디자이너 미리보기 값. 실행 중에는 SetBossDisplay/SetHealth로 갱신한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TD|UI|Boss HP|Display")
		FText BossName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TD|UI|Boss HP|Display",
		meta=(ClampMin="0"))
		int32 BossLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TD|UI|Boss HP|Display",
		meta=(ClampMin="0"))
		float Health = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TD|UI|Boss HP|Display",
		meta=(ClampMin="0"))
		float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="TD|UI|Boss HP", meta=(BindWidget))
		TObjectPtr<UTDTextBlock> BossNameText;

	UPROPERTY(BlueprintReadOnly, Category="TD|UI|Boss HP", meta=(BindWidget))
		TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(BlueprintReadOnly, Category="TD|UI|Boss HP", meta=(BindWidgetOptional))
		TObjectPtr<UTDTextBlock> BossLevelText;

	UPROPERTY(BlueprintReadOnly, Category="TD|UI|Boss HP", meta=(BindWidgetOptional))
		TObjectPtr<UTDTextBlock> HealthValueText;

	UFUNCTION()
		void RefreshFromViewModel();

private:
	void NormalizeHealth();
	void RefreshDisplay();
};
