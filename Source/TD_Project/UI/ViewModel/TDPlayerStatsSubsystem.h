#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "TDPlayerStatsSubsystem.generated.h"
class UTDPlayerStatsViewModel;
class ATDPlayerState;

/** 로컬 플레이어당 뷰모델 하나. 창을 닫아도 같은 인스턴스를 유지한다. */
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

 /** UI 생성 시 연결 확인. HP/MP/레벨/경험치 수치는 이벤트로만 갱신한다. */
 UFUNCTION(BlueprintCallable, Category="TD|UI|ViewModel")
 void RefreshSource();

private:
 UPROPERTY(Transient)
 TObjectPtr<UTDPlayerStatsViewModel> PlayerStatsViewModel;

 // 기존 Player 코드를 수정하지 않으므로 늦게 복제되거나 교체된 객체만 확인한다.
 TWeakObjectPtr<APlayerController> BoundController;
 TWeakObjectPtr<ATDPlayerState> BoundPlayerState;
 FTSTicker::FDelegateHandle SourceCheckHandle;
 bool CheckPlayerState(float DeltaTime);
};
