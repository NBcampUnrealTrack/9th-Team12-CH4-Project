#pragma once

#include "CoreMinimal.h"
#include "Components/CheckBox.h"
#include "TDCheckBox.generated.h"

class UTDCheckBoxStyleDA;

/** 체크 상태/이벤트는 UCheckBox 그대로, 공용 DA로 표시와 클릭 크기를 적용한다. */
UCLASS(BlueprintType, meta = (DisplayName = "TD CheckBox"))
class TD_PROJECT_API UTDCheckBox : public UCheckBox
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TD|CheckBox")
	void SetStyleData(UTDCheckBoxStyleDA* InStyleData);

	UFUNCTION(BlueprintPure, Category = "TD|CheckBox")
	UTDCheckBoxStyleDA* GetStyleData() const { return StyleData; }

	UFUNCTION(BlueprintCallable, Category = "TD|CheckBox")
	void RefreshStyle();

	virtual void SynchronizeProperties() override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|CheckBox")
	TObjectPtr<UTDCheckBoxStyleDA> StyleData;

	/** 기존 Render Scale을 보존하는 레이아웃에서만 사용. 새 위젯은 1 그대로. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|CheckBox", meta = (ClampMin = "0.01"))
	float StyleScale = 1.f;

	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	const FSlateBrush* GetIndicatorBrush() const;

	UPROPERTY(Transient)
	FCheckBoxStyle IndicatorStyle;
};
