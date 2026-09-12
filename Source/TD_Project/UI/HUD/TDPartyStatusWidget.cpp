#include "GameFramework/GameStateBase.h"
#include "UI/HUD/TDPartyStatusWidget.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/LocalPlayer.h"
#include "UI/Core/TDUIManagerSubsystem.h"
#include "UI/HUD/TDPartyMemberWidget.h"
#include "Party/TDPartyComponent.h"
#include "Player/TDPlayerState.h"
#include "Components/VerticalBox.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

UTDPartyComponent* UTDPartyStatusWidget::GetLocalParty() const
{
	const APlayerController* PC = GetOwningPlayer();
	ATDPlayerState* State = PC && PC->IsLocalController()
		                        ? PC->GetPlayerState<ATDPlayerState>()
		                        : nullptr;
	return State ? State->GetPartyComponent() : nullptr;
}

void UTDPartyStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshParty();
	// 숨겨진 상태에서도 다른 파티원의 가입/탈퇴와 PlayerState 교체를 감지한다.
	if (GetWorld()){
		GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::RefreshParty, .25f,
		                                       true);
	}
}

void UTDPartyStatusWidget::RefreshParty()
{
 UpdatePartySubscriptions();
	UTDPartyComponent* Party = GetLocalParty();
	if (BoundParty.Get() != Party){
		if (BoundParty.IsValid()){
			BoundParty->OnPartyInviteReceived.RemoveDynamic(this, &ThisClass::HandleInviteReceived);
		}
		BoundParty = Party;
		if (Party){
			Party->OnPartyInviteReceived.AddUniqueDynamic(this, &ThisClass::HandleInviteReceived);
		}
	}
	TArray<ATDPlayerState*> Members;
	if (Party && Party->IsInParty()){
		Members = Party->GetPartyMembers();
	}
	Members.RemoveAll([this](ATDPlayerState* State)
	{
		return !IsValid(State) || State == (GetOwningPlayer()
			                                    ? GetOwningPlayer()->PlayerState
			                                    : nullptr) || !State->HasSelectedCharacter();
	});
	// 기존 줄 순서를 유지하고 새 파티원만 끝에 붙여 목록 깜빡임을 방지한다.
	for (int32 Index = MemberRows.Num() - 1; Index >= 0; --Index){
		if (!MemberRows[Index] || !Members.Contains(MemberRows[Index]->GetMember())){
			if (MemberRows[Index]){
				MemberRows[Index]->SetMember(nullptr);
				MemberRows[Index]->RemoveFromParent();
			}
			MemberRows.RemoveAt(Index);
		}
	}
	if (MemberList && MemberWidgetClass){
		for (ATDPlayerState* State : Members){
			if (MemberRows.ContainsByPredicate([State](const UTDPartyMemberWidget* Row)
			{
				return Row && Row->GetMember() == State;
			})){
				continue;
			}
			if (UTDPartyMemberWidget* Row = CreateWidget<UTDPartyMemberWidget>(
					GetOwningPlayer(), MemberWidgetClass)){
				Row->SetMember(State);
				MemberList->AddChildToVerticalBox(Row);
				MemberRows.Add(Row);
			}
		}
	}
	for (UTDPartyMemberWidget* Row : MemberRows){
		if (Row){
			Row->RefreshMember();
		}
	}
	SetVisibility(MemberRows.IsEmpty()
		              ? ESlateVisibility::Collapsed
		              : ESlateVisibility::SelfHitTestInvisible);
}

void UTDPartyStatusWidget::NativeDestruct()
{
 for (const auto& Party : ObservedParties)
  if (Party.IsValid())
   Party->OnPartyChanged.RemoveDynamic(this, &ThisClass::QueuePartyRefresh);
 ObservedParties.Reset();
 if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(PartyChangeRefreshTimer);
	if (GetWorld()){
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	}
	if (BoundParty.IsValid()){
		BoundParty->OnPartyInviteReceived.RemoveDynamic(this, &ThisClass::HandleInviteReceived);
	}
	BoundParty.Reset();
	for (UTDPartyMemberWidget* Row : MemberRows){
		if (Row){
			Row->SetMember(nullptr);
			Row->RemoveFromParent();
		}
	}
	MemberRows.Reset();
	Super::NativeDestruct();
}

void UTDPartyStatusWidget::HandleInviteReceived(ATDPlayerState* Inviter)
{
	OnInviteReceived(Inviter);
	if (auto* LP = GetOwningLocalPlayer()){
		if (auto* UI = LP->GetSubsystem<UTDUIManagerSubsystem>()){
			UI->ShowPartyInvitation(Inviter);
		}
	}
}

void UTDPartyStatusWidget::InvitePlayer(ATDPlayerState* Target)
{
	if (UTDPartyComponent* Party = GetLocalParty()){
		if (IsValid(Target) && Target != (GetOwningPlayer()
			                                  ? GetOwningPlayer()->PlayerState
			                                  : nullptr) && Target->HasSelectedCharacter()){
			Party->ServerInvitePlayer(Target);
		}
	}
}

void UTDPartyStatusWidget::RespondToInvite(bool bAccept)
{
	if (UTDPartyComponent* Party = GetLocalParty()){
		Party->ServerRespondToInvite(bAccept);
	}
}

void UTDPartyStatusWidget::LeaveParty()
{
	if (UTDPartyComponent* Party = GetLocalParty()){
		Party->ServerLeaveParty();
	}
}

void UTDPartyStatusWidget::KickMember(ATDPlayerState* Target)
{
	if (UTDPartyComponent* Party = GetLocalParty()){
		if (Party->IsPartyLeader() && IsValid(Target)){
			Party->ServerKickMember(Target);
		}
	}
}

void UTDPartyStatusWidget::PromoteToLeader(ATDPlayerState* Target)
{
	if (UTDPartyComponent* Party = GetLocalParty()){
		if (Party->IsPartyLeader() && IsValid(Target)){
			Party->ServerPromoteToLeader(Target);
		}
	}
}


void UTDPartyStatusWidget::UpdatePartySubscriptions()
{
 TArray<TWeakObjectPtr<UTDPartyComponent>> Current;
 if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr)
 {
  for (APlayerState* State : GS->PlayerArray)
   if (const ATDPlayerState* Player = Cast<ATDPlayerState>(State))
    if (UTDPartyComponent* Party = Player->GetPartyComponent())
     Current.AddUnique(Party);
 }
 // 다른 파티원의 PartyId 변경도 감시한다. 내 PartyId는 탈퇴 시 그대로일 수 있다.
 for (const auto& Party : ObservedParties)
  if (Party.IsValid() && !Current.Contains(Party))
   Party->OnPartyChanged.RemoveDynamic(this, &ThisClass::QueuePartyRefresh);
 for (const auto& Party : Current)
  if (!ObservedParties.Contains(Party))
   Party->OnPartyChanged.AddUniqueDynamic(this, &ThisClass::QueuePartyRefresh);
 ObservedParties = MoveTemp(Current);
}

void UTDPartyStatusWidget::QueuePartyRefresh()
{
 if (UWorld* World = GetWorld(); World && !World->GetTimerManager().IsTimerActive(PartyChangeRefreshTimer))
 {
  // 같은 프레임의 여러 복제 알림을 모은 뒤 완성된 상태로 한 번 갱신한다.
  PartyChangeRefreshTimer = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RefreshParty);
 }
}
