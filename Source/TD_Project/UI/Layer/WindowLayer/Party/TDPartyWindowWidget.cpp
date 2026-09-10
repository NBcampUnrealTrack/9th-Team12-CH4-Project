#include "UI/Layer/WindowLayer/Party/TDPartyWindowWidget.h"
#include "GameFramework/PlayerController.h"
#include "UI/Layer/WindowLayer/Party/TDPartyRosterEntry.h"
#include "UI/Common/Typography/TDTextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "GameFramework/GameStateBase.h"
#include "Player/TDPlayerState.h"
#include "Party/TDPartyComponent.h"
#include "Settings/TDPartySettings.h"
#include "TimerManager.h"

UTDPartyComponent* UTDPartyWindowWidget::LocalParty() const
{
	auto* Self = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ATDPlayerState>() : nullptr;
	return Self ? Self->GetPartyComponent() : nullptr;
}

bool UTDPartyWindowWidget::CanInvite() const
{
	auto* P = LocalParty();
	return P && (!P->IsInParty() || (P->IsPartyLeader() && P->GetPartyMemberCount() < GetDefault<
		UTDPartySettings>()->MaxPartySize));
}

void UTDPartyWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (LeaveButton) LeaveButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestLeaveParty);
	PartyList->ClearChildren();
	OnlineList->ClearChildren();
	SearchBox->OnTextChanged.AddUniqueDynamic(this, &ThisClass::SearchChanged);
	AcceptButton->OnClicked.AddUniqueDynamic(this, &ThisClass::AcceptInvite);
	DeclineButton->OnClicked.AddUniqueDynamic(this, &ThisClass::DeclineInvite);

	RefreshRoster();
	GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::RefreshRoster, 10.0f,
	                                       true);
}

void UTDPartyWindowWidget::SyncRows(UVerticalBox* List,
                                    TArray<TObjectPtr<UTDPartyRosterEntry>>& Rows,
                                    const TArray<ATDPlayerState*>& States, bool bMember)
{
	for (int32 i = Rows.Num() - 1; i >= 0; --i){
		if (!Rows[i] || !States.Contains(Rows[i]->GetEntryState())){
			if (Rows[i]){
				Rows[i]->RemoveFromParent();
			}
			Rows.RemoveAt(i);
		}
	}
	for (auto* S : States){
		if (Rows.ContainsByPredicate([S](const UTDPartyRosterEntry* R)
		{
			return R && R->GetEntryState() == S;
		})){
			continue;
		}
		if (auto* R = CreateWidget<UTDPartyRosterEntry>(GetOwningPlayer(), EntryClass)){
			R->SetEntry(S, this, bMember);
			List->AddChildToVerticalBox(R);
			Rows.Add(R);
		}
	}
	for (UTDPartyRosterEntry* R : Rows){
		R->RefreshEntry(CanInvite(),
		                SentUntil.Contains(R->GetEntryState()));
	}
}

void UTDPartyWindowWidget::RefreshRoster()
{
	auto* Self = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ATDPlayerState>() : nullptr;
	if (CurrentSelf.Get() != Self){
		CurrentSelf = Self;
		SentUntil.Reset();
		IncomingInviter.Reset();
	}
	auto* P = LocalParty();
	auto* GS = GetWorld()->GetGameState();
	const double Now = GetWorld()->GetTimeSeconds();
	for (auto It = SentUntil.CreateIterator(); It; ++It){
		if (!It.Key().IsValid() || It.Value() <= Now || (It.Key()->GetPartyComponent() && It.Key()->
			GetPartyComponent()->IsInParty())){
			It.RemoveCurrent();
		}
	}
	TArray<ATDPlayerState*> Members, Candidates;
	if (P && P->IsInParty()){
		Members = P->GetPartyMembers();
	}
	const FString Query = SearchBox->GetText().ToString().TrimStartAndEnd();
	if (GS && Self && Self->HasSelectedCharacter()){
		for (APlayerState* PS : GS->PlayerArray){
			auto* S = Cast<ATDPlayerState>(PS);
			if (S && S != Self && !S->IsInactive() && S->HasSelectedCharacter() && S->
				GetPartyComponent() && !S->GetPartyComponent()->IsInParty() && S->GetPlayerName().
				Contains(Query)){
				Candidates.Add(S);
			}
		}
	}
	Candidates.Sort([](const ATDPlayerState& A, const ATDPlayerState& B)
	{
		return A.GetPlayerName() < B.GetPlayerName();
	});
	SyncRows(PartyList, PartyRows, Members, true);
	SyncRows(OnlineList, OnlineRows, Candidates, false);
	const bool bIncoming = IncomingInviter.IsValid() && Now < IncomingUntil && P && !P->IsInParty();
	if (!bIncoming){
		IncomingInviter.Reset();
	}
	OnLeaveAvailability(P && P->IsInParty());
	OnRosterPresentation(Members.Num(), GetDefault<UTDPartySettings>()->MaxPartySize,
	                     Candidates.Num(),
	                     P && P->IsInParty(), P && P->IsPartyLeader(), CanInvite(), bIncoming,
	                     IncomingInviter.IsValid()
		                     ? FText::FromString(IncomingInviter->GetPlayerName())
		                     : FText::GetEmpty());
}

void UTDPartyWindowWidget::RequestInvite(ATDPlayerState* Target)
{
	if (!CanInvite() || !IsValid(Target) || Target == CurrentSelf.Get() || !Target->
		HasSelectedCharacter() || !Target->GetPartyComponent() || Target->GetPartyComponent()->
		IsInParty() || SentUntil.Contains(Target)){
		return;
	}
	// 전송 사실만 표시한다. 서버의 초대 성공 응답은 기존 RPC에 없다.
	LocalParty()->ServerInvitePlayer(Target);
	SentUntil.Add(
			Target,
			GetWorld()->GetTimeSeconds() + GetDefault<UTDPartySettings>()->InviteExpireSeconds);
	RefreshRoster();
}

void UTDPartyWindowWidget::ShowInvitation(ATDPlayerState* Inviter)
{
	RefreshRoster();
	IncomingInviter = Inviter;
	IncomingUntil = GetWorld()->GetTimeSeconds() + GetDefault<UTDPartySettings>()->
			InviteExpireSeconds;
	RefreshRoster();
}

void UTDPartyWindowWidget::SearchChanged(const FText&) { RefreshRoster(); }

void UTDPartyWindowWidget::AcceptInvite()
{
	if (LocalParty() && IncomingInviter.IsValid()){
		LocalParty()->ServerRespondToInvite(true);
	}
	IncomingInviter.Reset();
	RefreshRoster();
}

void UTDPartyWindowWidget::DeclineInvite()
{
	if (LocalParty() && IncomingInviter.IsValid()){
		LocalParty()->ServerRespondToInvite(false);
	}
	IncomingInviter.Reset();
	RefreshRoster();
}

void UTDPartyWindowWidget::NativeDestruct()
{
	if (GetWorld()){
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	}
	SearchBox->OnTextChanged.RemoveDynamic(this, &ThisClass::SearchChanged);
	AcceptButton->OnClicked.RemoveDynamic(this, &ThisClass::AcceptInvite);
	DeclineButton->OnClicked.RemoveDynamic(this, &ThisClass::DeclineInvite);
	PartyRows.Reset();
	OnlineRows.Reset();
	SentUntil.Reset();
	IncomingInviter.Reset();
	if (LeaveButton) LeaveButton->OnClicked.RemoveDynamic(this, &ThisClass::RequestLeaveParty);
	Super::NativeDestruct();
}

void UTDPartyWindowWidget::RequestLeaveParty()
{
 if (UTDPartyComponent* Party = LocalParty(); Party && Party->IsInParty())
 {
  Party->ServerLeaveParty();
  RefreshRoster(); // 최종 목록은 서버 복제 후 정기 갱신에서 반영된다.
 }
}