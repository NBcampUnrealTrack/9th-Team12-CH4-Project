#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Styling/SlateTypes.h"
#include "TDButtonStyleDA.generated.h"


UCLASS(BlueprintType)
class TD_PROJECT_API UTDButtonStyleDA : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button Style")
	FButtonStyle DefaultStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button Style")
	bool bUseSelectedStyle = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button Style",
		meta = (EditCondition = "bUseSelectedStyle"))
	FButtonStyle SelectedStyle;

	UFUNCTION(BlueprintPure, Category = "Button Style")
	FButtonStyle GetStyle(bool bSelected) const;
};
