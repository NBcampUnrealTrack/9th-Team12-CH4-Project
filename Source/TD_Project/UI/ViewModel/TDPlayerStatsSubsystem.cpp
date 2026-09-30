#include "UI/ViewModel/TDPlayerStatsSubsystem.h"
#include "UI/ViewModel/TDPlayerStatsViewModel.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Player/TDPlayerState.h"
#include "Character/TDPlayerCharacter.h"

void UTDPlayerStatsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 PlayerStatsViewModel = NewObject<UTDPlayerStatsViewModel>(this);
 PlayerStatsViewModel->SetSource(nullptr);
 RefreshSource();
 SourceCheckHandle = FTSTicker::GetCoreTicker().AddTicker(
  FTickerDelegate::CreateUObject(this, &ThisClass::CheckPlayerState), 0.25f);
}

void UTDPlayerStatsSubsystem::Deinitialize()
{
 FTSTicker::GetCoreTicker().RemoveTicker(SourceCheckHandle);
 SourceCheckHandle.Reset();
 if (PlayerStatsViewModel) PlayerStatsViewModel->SetSource(nullptr);
 PlayerStatsViewModel = nullptr;
 BoundController.Reset();
 BoundPlayerState.Reset();
 Super::Deinitialize();
}

void UTDPlayerStatsSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
 if (!PlayerStatsViewModel) return;
 ATDPlayerState* NewPlayerState = IsValid(NewPlayerController)
  ? NewPlayerController->GetPlayerState<ATDPlayerState>() : nullptr;
 if (!IsValid(NewPlayerState)) NewPlayerState = nullptr;
 if (BoundController.Get() != NewPlayerController || BoundPlayerState.Get() != NewPlayerState
  || PlayerStatsViewModel->HasPlayerState != (NewPlayerState != nullptr))
 {
  BoundController = NewPlayerController;
  BoundPlayerState = NewPlayerState;
  PlayerStatsViewModel->SetSource(NewPlayerState);
 }
 // Controller/PlayerState가 그대로여도 Pawn은 부활이나 캐릭터 선택으로 바뀔 수 있다.
 ATDPlayerCharacter* Character = IsValid(NewPlayerController)
  && NewPlayerController->IsLocalController() && NewPlayerController->GetLocalPlayer() == GetLocalPlayer()
  ? Cast<ATDPlayerCharacter>(NewPlayerController->GetPawn()) : nullptr;
 PlayerStatsViewModel->SetDeathSource(Character);
}

void UTDPlayerStatsSubsystem::RefreshSource()
{
 PlayerControllerChanged(GetLocalPlayer()->GetPlayerController(GetWorld()));
}

bool UTDPlayerStatsSubsystem::CheckPlayerState(float DeltaTime)
{
 RefreshSource();
 return true;
}
