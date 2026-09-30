#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/TDQuestTypes.h"
#include "TDQuestMarkerWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDQuestMarkerWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable,
		Category = "TD|Quest")
	void SetMarkerData(
		const FTDQuestMarkerView& InData);

	UFUNCTION(BlueprintPure,
		Category = "TD|Quest")
	FTDQuestMarkerView GetMarkerData() const
	{
		return MarkerData;
	}

protected:
	UFUNCTION(BlueprintImplementableEvent,
		Category = "TD|Quest",
		DisplayName = "On Marker Data Changed")
	void BP_OnMarkerDataChanged(
		const FTDQuestMarkerView& InData);

private:
	UPROPERTY(Transient)
	FTDQuestMarkerView MarkerData;
};