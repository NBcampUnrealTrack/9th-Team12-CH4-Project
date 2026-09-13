#pragma once
#include "CoreMinimal.h"
#include "UI/Layer/WindowLayer/Common/WindowBase/TDWindowBaseWidget.h"
#include "TDPartyWindowWidget.generated.h"
class ATDPlayerState;
class UTDPartyComponent;
class UTDPartyRosterEntry;
class UTDTextBlock;
class UVerticalBox;
class UEditableTextBox;
class UButton;
class UHorizontalBox;

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDPartyWindowWidget : public UTDWindowBaseWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="TD|UI|Party")
		void RefreshRoster();
	UFUNCTION(BlueprintCallable, Category="TD|UI|Party")
		void RequestInvite(ATDPlayerState* Target);
	void ShowInvitation(ATDPlayerState* Inviter);
	UFUNCTION(BlueprintCallable, Category="TD|UI|Party")
	void RequestLeaveParty();

protected:
	virtual void NativeConstruct() override;
	/** 표시할 수치/상태만 보낸다. 안내 문구와 가시성은 WBP에서 관리한다. */
	UFUNCTION(BlueprintImplementableEvent, Category="TD|UI|Party")
		void OnRosterPresentation(int32 MemberCount, int32 Capacity, int32 CandidateCount,
		                          bool bHasParty, bool bLeader, bool bInviteAllowed, bool bIncoming,
		                          const FText& InviterName);
	virtual void NativeDestruct() override;
	UFUNCTION(BlueprintImplementableEvent, Category="TD|UI|Party")
	void OnLeaveAvailability(bool bCanLeave);
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UButton> LeaveButton;
	UPROPERTY(EditDefaultsOnly, Category="TD|UI|Party")
		TSubclassOf<UTDPartyRosterEntry> EntryClass;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
		TObjectPtr<UVerticalBox> PartyList;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
		TObjectPtr<UVerticalBox> OnlineList;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
		TObjectPtr<UEditableTextBox> SearchBox;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
		TObjectPtr<UTDTextBlock> PartyCountText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
		TObjectPtr<UTDTextBlock> OnlineCountText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
		TObjectPtr<UTDTextBlock> EmptyPartyText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
		TObjectPtr<UTDTextBlock> EmptyOnlineText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
		TObjectPtr<UTDTextBlock> StatusText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
		TObjectPtr<UTDTextBlock> InvitationText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
		TObjectPtr<UHorizontalBox> InvitationPanel;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
		TObjectPtr<UButton> AcceptButton;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
		TObjectPtr<UButton> DeclineButton;

private:
	FTimerHandle RefreshTimer;
 FTimerHandle PartyChangeRefreshTimer;
 TArray<TWeakObjectPtr<UTDPartyComponent>> ObservedParties;
 void UpdatePartySubscriptions();
 UFUNCTION()
 void QueuePartyRefresh();
	TWeakObjectPtr<ATDPlayerState> CurrentSelf;
	TWeakObjectPtr<ATDPlayerState> IncomingInviter;
	double IncomingUntil = 0;
	TMap<TWeakObjectPtr<ATDPlayerState>, double> SentUntil;
	UPROPERTY(Transient)
		TArray<TObjectPtr<UTDPartyRosterEntry>> PartyRows;
	UPROPERTY(Transient)
		TArray<TObjectPtr<UTDPartyRosterEntry>> OnlineRows;
	UTDPartyComponent* LocalParty() const;
	bool CanInvite() const;
	void SyncRows(UVerticalBox* List, TArray<TObjectPtr<UTDPartyRosterEntry>>& Rows,
	              const TArray<ATDPlayerState*>& States, bool bMember);
	UFUNCTION()
		void SearchChanged(const FText& Text);
	UFUNCTION()
		void AcceptInvite();
	UFUNCTION()
		void DeclineInvite();
};
