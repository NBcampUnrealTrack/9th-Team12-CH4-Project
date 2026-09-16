#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TDLowHealthStyleDA.generated.h"

UCLASS(BlueprintType)
class TD_PROJECT_API UTDLowHealthStyleDA : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Low Health")
		bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Low Health",
		meta=(ClampMin="0.01", ClampMax="1.0"))
		float StartHealthPercent = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Low Health",
		meta=(ClampMin="0.0", ClampMax="1.0"))
		float PulseHealthPercent = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Low Health")
		FLinearColor Color = FLinearColor(0.65f, 0.005f, 0.01f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Low Health",
		meta=(ClampMin="0.0", ClampMax="1.0"))
		float MaxOpacity = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Low Health", meta=(ClampMin="0.01"))
		float FadeInSeconds = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Low Health", meta=(ClampMin="0.01"))
		float FadeOutSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Low Health|Pulse",
		meta=(ClampMin="0.0", ClampMax="0.5"))
		float PulseAmount = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Low Health|Pulse", meta=(ClampMin="0.2"))
		float PulsePeriodSeconds = 1.2f;
};
