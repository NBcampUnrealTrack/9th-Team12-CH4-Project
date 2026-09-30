#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TDPartyComponent.generated.h"

class ATDPlayerState;

/** 파티 구성이 바뀌었을 때(가입·탈퇴·리더 변경). 파티 UI 가 이걸 받아 목록을 다시 그린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnPartyChanged);

/**
 * 파티 초대를 받았을 때. 초대받은 쪽에서만 불린다.
 *
 * UI 가 수락/거절 창을 띄운다. 응답은 ServerRespondToInvite 로 돌려보낸다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOnPartyInviteReceived, ATDPlayerState*, Inviter);

/**
 * 파티 상태를 담는 컴포넌트. `ATDPlayerState` 에 붙는다.
 *
 * ── 파티 목록을 따로 복제하지 않는 이유 (D70) ──
 * 여기 있는 것은 `PartyId` 와 리더 여부뿐이다. "누가 같은 파티인가" 는
 * `GameState->PlayerArray` 에서 같은 PartyId 를 모아 구한다.
 *
 * 파티 목록을 별도 자료구조로 만들면 같은 정보가 두 곳에 존재하게 되고,
 * 누가 접속을 끊었을 때 양쪽을 다 고쳐야 한다. 그리고 파티 UI 가 필요로 하는 값은
 * 이미 전부 복제되고 있다.
 *
 *   이름·레벨·직업    PlayerState
 *   체력/최대체력      ASC 가 Mixed 모드라 어트리뷰트는 전원에게 간다
 *   전투력             ReplicatedCombatPower
 *   현재 존            CurrentZoneId
 *
 * ── 파티 자체를 액터로 만들지 않은 이유 ──
 * 파티는 "주인이 없는 상태" 라 서브시스템이 어울리지만, 초대·수락은 RPC 라
 * 액터나 컴포넌트에만 둘 수 있다(D46). 그래서 각자가 자기 몫을 들고,
 * 파티는 같은 PartyId 를 가진 사람들의 집합으로만 존재한다.
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDPartyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDPartyComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── 조회 ──────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "TD|Party")
	bool IsInParty() const { return PartyId.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "TD|Party")
	bool IsPartyLeader() const { return PartyId.IsValid() && bIsLeader; }

	UFUNCTION(BlueprintPure, Category = "TD|Party")
	FGuid GetPartyId() const { return PartyId; }

	/**
	 * 같은 파티원 전부. 자기 자신도 포함한다.
	 *
	 * 매번 PlayerArray 를 순회한다. 30명 이하라 비용이 없고, 캐시를 두면
	 * 누가 접속을 끊었을 때 갱신을 빠뜨릴 위험이 생긴다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Party")
	TArray<ATDPlayerState*> GetPartyMembers() const;

	UFUNCTION(BlueprintPure, Category = "TD|Party")
	int32 GetPartyMemberCount() const;

	/** 파티장. 파티가 없으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "TD|Party")
	ATDPlayerState* GetPartyLeader() const;

	/**
	 * 인원수에 따른 경험치 가산율(0.3 이면 +30%).
	 *
	 * 값은 UTDPartySettings 에서 읽는다. 몬스터 처치 경험치에 곱해 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Party")
	float GetExpBonusRate() const;

	/**
	 * 몬스터 처치 경험치를 파티에 나눠준다. **서버 전용.**
	 *
	 * 전투 담당이 부를 함수다. 파티가 없으면 자기 혼자 받으므로
	 * "파티인지 확인하고 갈라 부르는" 코드가 필요 없다.
	 *
	 *   Killer->GetPartyComponent()->AwardKillExp(MonsterRow.ExpReward);
	 *
	 * 인원 보너스가 자동으로 적용된다. UTDProgressionComponent::AddExp 를 직접 부르면
	 * 보너스가 빠지므로, 처치 보상은 반드시 이 경로를 쓴다.
	 *
	 * **같은 존에 있는 파티원만** 받는다. 다른 존에서 놀고 있는데 경험치가
	 * 들어오면 파티에 이름만 올려두는 것이 이득이 된다.
	 *
	 * @return 실제로 경험치를 받은 인원 수.
	 */
	int32 AwardKillExp(int32 BaseAmount);

	/**
	 * 몬스터 처치 골드를 파티에 나눠준다. **서버 전용.** AwardKillExp 의 짝이다.
	 *
	 * ── 경험치와 달리 나눈다 ──
	 * 경험치는 각자 전액을 받고 인원 보너스까지 붙지만, 골드는 인원수로 **나눈다.**
	 * 경험치는 얼마를 뿌려도 캐릭터 성장 속도만 달라지지만, 골드는 경제라
	 * 같은 방식으로 주면 파티를 맺는 것만으로 돈이 불어난다.
	 *
	 * 나머지는 버린다 — 100 골드를 3명이 나누면 33씩이고 1은 사라진다.
	 * 누구에게 줄지 정하는 규칙이 사람마다 다르게 보이는 것보다 낫다.
	 *
	 * 경험치와 같이 **같은 존에 있는 파티원만** 받는다.
	 *
	 * @return 실제로 골드를 받은 인원 수.
	 */
	int32 AwardKillGold(int32 BaseAmount);

	// ── 요청 (클라이언트가 부른다) ────────────────────────

	/**
	 * 상대를 파티에 초대한다. 파티가 없으면 이 요청으로 새로 만들어진다.
	 *
	 * 파티가 있는 상태에서는 **리더만** 초대할 수 있다. 아무나 초대하면
	 * 리더가 모르는 사이에 인원이 차서 정작 부르려던 사람을 못 부른다.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Party")
	void ServerInvitePlayer(ATDPlayerState* Target);

	/** 받은 초대에 응답한다. 만료됐으면 조용히 무시된다. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Party")
	void ServerRespondToInvite(bool bAccept);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Party")
	void ServerLeaveParty();

	/** 파티장만 가능. 자기 자신은 추방할 수 없다(나가려면 LeaveParty). */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Party")
	void ServerKickMember(ATDPlayerState* Target);

	/** 파티장만 가능. 리더 자리를 넘긴다. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Party")
	void ServerPromoteToLeader(ATDPlayerState* Target);

	// ── 알림 ──────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "TD|Party")
	FTDOnPartyChanged OnPartyChanged;

	UPROPERTY(BlueprintAssignable, Category = "TD|Party")
	FTDOnPartyInviteReceived OnPartyInviteReceived;

	/** 서버 전용. 접속 종료 시 GameMode 가 부른다. */
	void HandleOwnerLogout();

protected:
	/** 초대받은 쪽에 알린다. 소유 클라이언트에게만 간다. */
	UFUNCTION(Client, Reliable)
	void ClientReceiveInvite(ATDPlayerState* Inviter);

private:
	/** 서버 전용. 파티에서 빼고, 남은 인원을 정리한다. */
	void RemoveFromParty();

	/** 서버 전용. PartyId 를 설정하고 알린다. */
	void SetPartyState(const FGuid& NewPartyId, bool bNewIsLeader);

	/**
	 * 파티에 한 명만 남으면 해체한다.
	 *
	 * 혼자 남은 파티를 유지하면 "파티 중" 표시가 계속 뜨고, 경험치 보너스 계산도
	 * 1명짜리 파티라는 예외를 다뤄야 한다.
	 */
	void DissolveIfAlone(const FGuid& TargetPartyId);

	ATDPlayerState* GetOwnerPlayerState() const;

	UFUNCTION()
	void OnRep_PartyState();

	/**
	 * 소속 파티. 비어 있으면 파티 없음.
	 *
	 * 전원에게 복제한다 — 남이 파티 중인지 알아야 초대 UI 에서 걸러낼 수 있고,
	 * 16바이트라 비용이 없다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_PartyState)
	FGuid PartyId;

	UPROPERTY(ReplicatedUsing = OnRep_PartyState)
	bool bIsLeader = false;

	// ── 서버 전용 상태 ────────────────────────────────────
	// 초대는 받은 사람만 알면 되므로 복제하지 않는다.

	UPROPERTY()
	TWeakObjectPtr<ATDPlayerState> PendingInviter;

	/** 초대받은 시각. 만료 검사에 쓴다. */
	double PendingInviteTime = 0.0;
};
