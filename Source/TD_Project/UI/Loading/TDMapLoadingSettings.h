#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TDMapLoadingSettings.generated.h"

class UUserWidget;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="TD Map Loading"))
class TD_PROJECT_API UTDMapLoadingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, Category="Loading Screen")
		bool bEnabled = true;

	UPROPERTY(Config, EditAnywhere, Category="Loading Screen")
		TSoftClassPtr<UUserWidget> LoadingWidgetClass;
};
