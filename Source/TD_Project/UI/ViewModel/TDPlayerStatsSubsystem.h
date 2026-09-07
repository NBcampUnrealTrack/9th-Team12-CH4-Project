#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "TDPlayerStatsSubsystem.generated.h"
class UTDPlayerStatsViewModel;
class ATDPlayerState;

UCLASS()
class TD_PROJECT_API UTDPlayerStatsSubsystem : public ULocalPlayerSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

 UFUNCTION(BlueprintPure, Category="TD|UI|ViewModel")
 UTDPlayerStatsViewModel* GetPlayerStatsViewModel() const { return PlayerStatsViewModel; }

 UFUNCTION(BlueprintCallable, Category="TD|UI|ViewModel")
 void RefreshSource();

private:
 UPROPERTY(Transient)
 TObjectPtr<UTDPlayerStatsViewModel> PlayerStatsViewModel;

 TWeakObjectPtr<APlayerController> BoundController;
 TWeakObjectPtr<ATDPlayerState> BoundPlayerState;
 FTSTicker::FDelegateHandle SourceCheckHandle;
 bool CheckPlayerState(float DeltaTime);
};
