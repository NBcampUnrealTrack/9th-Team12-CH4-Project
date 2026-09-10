#include "UI/Layer/WindowLayer/Party/TDPartyRosterEntry.h"
#include "GameFramework/PlayerController.h"
#include "UI/Layer/WindowLayer/Party/TDPartyWindowWidget.h"
#include "UI/Common/Typography/TDTextBlock.h"
#include "Components/ProgressBar.h"
#include "UI/Common/TDProgressBarStyleDA.h"
#include "UI/ViewModel/TDPlayerStatsViewModel.h"
#include "Player/TDPlayerState.h"
#include "Party/TDPartyComponent.h"
#include "Components/Image.h"
#include "Components/Button.h"

void UTDPartyRosterEntry::NativeConstruct() { Super::NativeConstruct(); if (InviteButton) InviteButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleInvite); }
void UTDPartyRosterEntry::SetEntry(ATDPlayerState* InState, UTDPartyWindowWidget* InOwner, bool bMember)
{
 State=InState; OwnerWindow=InOwner; bPartyMember=bMember; HealthAnimation.Reset();
 if (!ViewModel) ViewModel=NewObject<UTDPlayerStatsViewModel>(this);
 ViewModel->SetSource(InState);
}
void UTDPartyRosterEntry::RefreshEntry(bool bCanInvite, bool bPending)
{
 if (!ViewModel || !State.IsValid()) return;
 ViewModel->RefreshAll();
 const auto* Party=State->GetPartyComponent();
 const FTDProgressBarStyle Style=HealthBarStyleData ? HealthBarStyleData->Style : FTDProgressBarStyle();
 HealthAnimation.SetValue(ViewModel->Health,ViewModel->MaxHealth,ViewModel->HealthPercent,bPartyMember,Style);
 OnEntryPresentation(ViewModel,bPartyMember,Party && Party->IsPartyLeader(),
  GetOwningPlayer() && GetOwningPlayer()->PlayerState==State.Get(),bCanInvite && !bPending,bPending);
 ApplyHealthVisuals();
}
void UTDPartyRosterEntry::HandleInvite() { if (OwnerWindow.IsValid()) OwnerWindow->RequestInvite(State.Get()); }
void UTDPartyRosterEntry::NativeDestruct()
{
 if (InviteButton) InviteButton->OnClicked.RemoveDynamic(this,&ThisClass::HandleInvite);
 if (ViewModel) ViewModel->SetSource(nullptr);
 HealthAnimation.Reset();
 Super::NativeDestruct();
}
void UTDPartyRosterEntry::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
 Super::NativeTick(Geometry, DeltaSeconds);
 if (!bPartyMember || !HealthAnimation.IsActive()) return;
 HealthAnimation.Advance(DeltaSeconds);
 ApplyHealthVisuals();
}
void UTDPartyRosterEntry::ApplyHealthVisuals()
{
 const FTDProgressBarStyle Style=HealthBarStyleData ? HealthBarStyleData->Style : FTDProgressBarStyle();
 OnHealthPresentation(HealthAnimation.DisplayPercent,HealthAnimation.TrailPercent,
  HealthAnimation.FlashAlpha*FMath::Clamp(Style.FlashOpacity,0.f,1.f),Style.FillTint,Style.TrailTint,Style.FlashTint);
}

