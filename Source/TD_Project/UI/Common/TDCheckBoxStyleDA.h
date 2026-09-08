#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Styling/SlateTypes.h"
#include "TDCheckBoxStyleDA.generated.h"

/** 체크 상태별 이미지와 클릭 영역을 여러 TD CheckBox에서 공유한다. */
UCLASS(BlueprintType)
class TD_PROJECT_API UTDCheckBoxStyleDA : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 각 이미지의 Image Size는 보이는 네모의 크기다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CheckBox Style")
	FCheckBoxStyle Style;

	/** DA에서 지정하는 최소 클릭 영역. 0이면 이미지 크기를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CheckBox Style", meta = (ClampMin = "0"))
	FVector2D HitAreaSize = FVector2D::ZeroVector;

	UFUNCTION(BlueprintPure, Category = "CheckBox Style")
	FCheckBoxStyle GetStyle() const { return Style; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif
};
