#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDPartyStatusWidget.generated.h"

class ATDPlayerState;
class UTDPartyComponent;
class UTDPartyMemberWidget;
class UVerticalBox;

/** 서버 상태는 변경하지 않고 기존 PartyComponent의 조회/RPC만 사용한다. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDPartyStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="TD|UI|Party")
		void RefreshParty();
	UFUNCTION(BlueprintCallable, Category="TD|UI|Party")
		void InvitePlayer(ATDPlayerState* Target);
	UFUNCTION(BlueprintCallable, Category="TD|UI|Party")
		void RespondToInvite(bool bAccept);
	UFUNCTION(BlueprintCallable, Category="TD|UI|Party")
		void LeaveParty();
	UFUNCTION(BlueprintCallable, Category="TD|UI|Party")
		void KickMember(ATDPlayerState* Target);
	UFUNCTION(BlueprintCallable, Category="TD|UI|Party")
		void PromoteToLeader(ATDPlayerState* Target);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="TD|UI|Party")
		TSubclassOf<UTDPartyMemberWidget> MemberWidgetClass;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
		TObjectPtr<UVerticalBox> MemberList;
	/** 초대 팝업의 디자인/표시는 WBP에서 연결한다. */
	UFUNCTION(BlueprintImplementableEvent, Category="TD|UI|Party")
		void OnInviteReceived(ATDPlayerState* Inviter);

private:
	FTimerHandle RefreshTimer;
	TWeakObjectPtr<UTDPartyComponent> BoundParty;
	UPROPERTY(Transient)
		TArray<TObjectPtr<UTDPartyMemberWidget>> MemberRows;
	UTDPartyComponent* GetLocalParty() const;
	UFUNCTION()
		void HandleInviteReceived(ATDPlayerState* Inviter);
};
