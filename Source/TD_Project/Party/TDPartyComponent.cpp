#include "Party/TDPartyComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Items/TDInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/TDPlayerState.h"
#include "Settings/TDPartySettings.h"
#include "Stats/TDProgressionComponent.h"

UTDPartyComponent::UTDPartyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UTDPartyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 조건을 걸지 않는다. 남이 파티 중인지 알아야 초대 UI 에서 걸러낼 수 있다.
	DOREPLIFETIME(UTDPartyComponent, PartyId);
	DOREPLIFETIME(UTDPartyComponent, bIsLeader);
}

ATDPlayerState* UTDPartyComponent::GetOwnerPlayerState() const
{
	return Cast<ATDPlayerState>(GetOwner());
}

// ── 조회 ──────────────────────────────────────────────────

TArray<ATDPlayerState*> UTDPartyComponent::GetPartyMembers() const
{
	TArray<ATDPlayerState*> Members;

	if (!PartyId.IsValid())
	{
		// 파티가 없으면 자기 자신만. 호출한 쪽이 빈 배열과 1인 파티를 구분하지 않아도 되도록.
		if (ATDPlayerState* Self = GetOwnerPlayerState())
		{
			Members.Add(Self);
		}
		return Members;
	}

	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (GameState == nullptr)
	{
		return Members;
	}

	// 매번 순회한다. 30명 이하라 비용이 없고, 캐시를 두면 누가 접속을 끊었을 때
	// 갱신을 빠뜨려 유령 파티원이 남는다.
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		ATDPlayerState* TDPlayerState = Cast<ATDPlayerState>(PlayerState);
		if (TDPlayerState == nullptr)
		{
			continue;
		}

		const UTDPartyComponent* OtherParty = TDPlayerState->GetPartyComponent();
		if (OtherParty != nullptr && OtherParty->PartyId == PartyId)
		{
			Members.Add(TDPlayerState);
		}
	}

	return Members;
}

int32 UTDPartyComponent::GetPartyMemberCount() const
{
	return GetPartyMembers().Num();
}

ATDPlayerState* UTDPartyComponent::GetPartyLeader() const
{
	if (!PartyId.IsValid())
	{
		return nullptr;
	}

	for (ATDPlayerState* Member : GetPartyMembers())
	{
		const UTDPartyComponent* MemberParty = Member ? Member->GetPartyComponent() : nullptr;
		if (MemberParty != nullptr && MemberParty->bIsLeader)
		{
			return Member;
		}
	}

	return nullptr;
}

float UTDPartyComponent::GetExpBonusRate() const
{
	const UTDPartySettings* Settings = GetDefault<UTDPartySettings>();
	if (Settings == nullptr || Settings->ExpBonusByMemberCount.IsEmpty())
	{
		return 0.f;
	}

	const int32 Count = GetPartyMemberCount();
	if (Count <= 0)
	{
		return 0.f;
	}

	// 배열 인덱스는 인원수 - 1 이다. 인원이 배열보다 많으면 마지막 값을 쓴다 —
	// MaxPartySize 를 늘리고 배열을 안 늘렸다고 보너스가 0 이 되면 안 된다.
	const int32 Index = FMath::Min(Count - 1, Settings->ExpBonusByMemberCount.Num() - 1);
	return Settings->ExpBonusByMemberCount[Index];
}

int32 UTDPartyComponent::AwardKillExp(int32 BaseAmount)
{
	const ATDPlayerState* Self = GetOwnerPlayerState();
	if (Self == nullptr || !Self->HasAuthority() || BaseAmount <= 0)
	{
		return 0;
	}

	const float BonusRate = GetExpBonusRate();
	const int32 FinalAmount = FMath::RoundToInt(BaseAmount * (1.f + BonusRate));

	// 죽인 사람이 있는 존을 기준으로 삼는다. 다른 존의 파티원은 제외한다 —
	// 그러지 않으면 파티에 이름만 올려두고 노는 것이 이득이 된다.
	const FGameplayTag KillZone = Self->GetCurrentZoneId();

	int32 AwardedCount = 0;

	for (ATDPlayerState* Member : GetPartyMembers())
	{
		if (Member == nullptr || Member->GetCurrentZoneId() != KillZone)
		{
			continue;
		}

		if (UTDProgressionComponent* Progression = Member->GetProgressionComponent())
		{
			Progression->AddExp(FinalAmount);
			++AwardedCount;
		}
	}

	return AwardedCount;
}

int32 UTDPartyComponent::AwardKillGold(int32 BaseAmount)
{
	const ATDPlayerState* Self = GetOwnerPlayerState();
	if (Self == nullptr || !Self->HasAuthority() || BaseAmount <= 0)
	{
		return 0;
	}

	const FGameplayTag KillZone = Self->GetCurrentZoneId();

	// 받을 사람을 먼저 센다. 나눗셈을 하려면 인원이 확정돼야 하는데,
	// 존이 다른 파티원은 제외되므로 GetPartyMembers().Num() 을 그대로 쓸 수 없다.
	TArray<ATDPlayerState*> Receivers;
	for (ATDPlayerState* Member : GetPartyMembers())
	{
		if (Member != nullptr && Member->GetCurrentZoneId() == KillZone)
		{
			Receivers.Add(Member);
		}
	}

	if (Receivers.Num() == 0)
	{
		return 0;
	}

	// 경험치와 달리 나눈다. 인원 보너스도 없다 — 그러면 파티를 맺는 것만으로 돈이 불어난다.
	const int32 Share = BaseAmount / Receivers.Num();
	if (Share <= 0)
	{
		// 1 골드를 3명이 나누면 0 이다. 아무도 못 받는 편이 낫다 —
		// 누구 하나에게 몰아주면 그 규칙을 또 설명해야 한다.
		return 0;
	}

	int32 AwardedCount = 0;

	for (ATDPlayerState* Member : Receivers)
	{
		if (UTDInventoryComponent* Inventory = Member->GetInventoryComponent())
		{
			Inventory->AddGold(Share);
			++AwardedCount;
		}
	}

	return AwardedCount;
}

// ── 초대 ──────────────────────────────────────────────────

void UTDPartyComponent::ServerInvitePlayer_Implementation(ATDPlayerState* Target)
{
	ATDPlayerState* Self = GetOwnerPlayerState();
	if (Self == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("파티 초대 실패: 보낸 쪽 PlayerState 를 찾지 못했다."));
		return;
	}

	if (Target == nullptr)
	{
		// 클라이언트는 대상을 지정해 보냈는데 서버에 null 로 도착한 경우다.
		// 그 사이 접속을 끊었거나, 로그에 "Forged object" 경고가 함께 떴다면
		// Live Coding 이 클래스를 갈아버려 RPC 파라미터의 타입 검증이 실패한 것이다.
		// 후자는 코드 문제가 아니므로 **에디터를 완전히 종료하고 전체 빌드**해야 한다.
		UE_LOG(LogTemp, Warning,
			TEXT("파티 초대 실패: 대상이 서버에 null 로 도착했다. "
			     "바로 위에 'Forged object' 경고가 있다면 Live Coding 탓이므로 "
			     "에디터를 끄고 전체 빌드할 것."));
		return;
	}

	if (Target == Self)
	{
		UE_LOG(LogTemp, Warning, TEXT("파티 초대 실패: 자기 자신은 초대할 수 없다."));
		return;
	}

	UTDPartyComponent* TargetParty = Target->GetPartyComponent();
	if (TargetParty == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("파티 초대 실패: %s 의 PartyComponent 가 없다."), *Target->GetPlayerName());
		return;
	}

	if (TargetParty->IsInParty())
	{
		UE_LOG(LogTemp, Log, TEXT("파티 초대 거부: %s 는 이미 파티에 속해 있다."),
			*Target->GetPlayerName());
		return;
	}

	// 파티가 이미 있으면 리더만 초대할 수 있다. 아무나 초대하면 리더가 모르는 사이에
	// 인원이 차서 정작 부르려던 사람을 못 부르게 된다.
	if (PartyId.IsValid() && !bIsLeader)
	{
		UE_LOG(LogTemp, Log, TEXT("파티 초대 거부: %s 는 파티장이 아니다."), *Self->GetPlayerName());
		return;
	}

	const UTDPartySettings* Settings = GetDefault<UTDPartySettings>();
	const int32 MaxSize = Settings != nullptr ? Settings->MaxPartySize : 4;

	if (PartyId.IsValid() && GetPartyMemberCount() >= MaxSize)
	{
		UE_LOG(LogTemp, Log, TEXT("파티 초대 거부: 정원 %d명이 찼다."), MaxSize);
		return;
	}

	// 거리 제한이 걸려 있으면 확인한다. 0 이면 제한 없음.
	if (Settings != nullptr && Settings->MaxInviteDistance > 0.f)
	{
		const APawn* SelfPawn = Self->GetPawn();
		const APawn* TargetPawn = Target->GetPawn();

		if (SelfPawn != nullptr && TargetPawn != nullptr)
		{
			const float Distance = FVector::Dist(
				SelfPawn->GetActorLocation(), TargetPawn->GetActorLocation());

			if (Distance > Settings->MaxInviteDistance)
			{
				UE_LOG(LogTemp, Log, TEXT("파티 초대 거부: %s 가 너무 멀다 (%.0f)."),
					*Target->GetPlayerName(), Distance);
				return;
			}
		}
	}

	// 초대는 받은 사람만 알면 되므로 복제하지 않고 서버가 들고 있는다.
	TargetParty->PendingInviter = Self;
	TargetParty->PendingInviteTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	TargetParty->ClientReceiveInvite(Self);

	UE_LOG(LogTemp, Log, TEXT("파티 초대: %s → %s"),
		*Self->GetPlayerName(), *Target->GetPlayerName());
}

void UTDPartyComponent::ClientReceiveInvite_Implementation(ATDPlayerState* Inviter)
{
	// 문구와 창은 UI 가 만든다. 여기서는 신호만 보낸다.
	OnPartyInviteReceived.Broadcast(Inviter);
}

void UTDPartyComponent::ServerRespondToInvite_Implementation(bool bAccept)
{
	ATDPlayerState* Self = GetOwnerPlayerState();
	ATDPlayerState* Inviter = PendingInviter.Get();

	const FString SelfName = Self != nullptr ? Self->GetPlayerName() : TEXT("(알 수 없음)");

	// 응답 한 번에 초대는 소멸한다. 거절이든 만료든 같다.
	PendingInviter = nullptr;

	if (Self == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("파티 응답 실패: PlayerState 를 찾지 못했다."));
		return;
	}

	if (!bAccept)
	{
		UE_LOG(LogTemp, Log, TEXT("파티 거절: %s"), *SelfName);
		return;
	}

	if (Inviter == nullptr)
	{
		// 가장 흔한 경우다. 초대받지 않은 사람이 수락을 눌렀거나,
		// 초대한 사람이 그 사이 접속을 끊었다.
		UE_LOG(LogTemp, Warning,
			TEXT("파티 수락 실패: %s 에게 대기 중인 초대가 없다. "
			     "초대받은 쪽에서 수락했는지 확인할 것."), *SelfName);
		return;
	}

	const UTDPartySettings* Settings = GetDefault<UTDPartySettings>();
	const float ExpireSeconds = Settings != nullptr ? Settings->InviteExpireSeconds : 60.f;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	if (Now - PendingInviteTime > ExpireSeconds)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("파티 수락 실패: 초대가 만료됐다. (%.0f초 경과 / 제한 %.0f초)"),
			Now - PendingInviteTime, ExpireSeconds);
		return;
	}

	if (IsInParty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("파티 수락 실패: %s 는 이미 파티에 속해 있다."), *SelfName);
		return;
	}

	// 초대한 쪽 사정이 그새 바뀌었을 수 있다. 수락 시점에 다시 확인한다 —
	// 초대해 놓고 다른 파티에 들어갔거나, 그 사이 정원이 찼을 수 있다.
	UTDPartyComponent* InviterParty = Inviter->GetPartyComponent();
	if (InviterParty == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("파티 수락 실패: 초대한 %s 의 PartyComponent 가 없다."),
			*Inviter->GetPlayerName());
		return;
	}

	const int32 MaxSize = Settings != nullptr ? Settings->MaxPartySize : 4;

	if (InviterParty->PartyId.IsValid())
	{
		if (InviterParty->GetPartyMemberCount() >= MaxSize)
		{
			UE_LOG(LogTemp, Log, TEXT("파티 수락 실패: 그 사이 정원이 찼다."));
			return;
		}

		SetPartyState(InviterParty->PartyId, /*bNewIsLeader=*/ false);
	}
	else
	{
		// 초대한 쪽도 파티가 없었으므로 지금 만든다. 초대한 사람이 리더가 된다.
		const FGuid NewPartyId = FGuid::NewGuid();

		InviterParty->SetPartyState(NewPartyId, /*bNewIsLeader=*/ true);
		SetPartyState(NewPartyId, /*bNewIsLeader=*/ false);
	}

	UE_LOG(LogTemp, Log, TEXT("파티 가입: %s → %s 의 파티 (%d명)"),
		*Self->GetPlayerName(), *Inviter->GetPlayerName(), GetPartyMemberCount());
}

// ── 탈퇴·추방 ─────────────────────────────────────────────

void UTDPartyComponent::ServerLeaveParty_Implementation()
{
	RemoveFromParty();
}

void UTDPartyComponent::ServerKickMember_Implementation(ATDPlayerState* Target)
{
	if (!IsPartyLeader() || Target == nullptr || Target == GetOwnerPlayerState())
	{
		return;
	}

	UTDPartyComponent* TargetParty = Target->GetPartyComponent();
	if (TargetParty == nullptr || TargetParty->PartyId != PartyId)
	{
		return;
	}

	TargetParty->RemoveFromParty();

	UE_LOG(LogTemp, Log, TEXT("파티 추방: %s"), *Target->GetPlayerName());
}

void UTDPartyComponent::ServerPromoteToLeader_Implementation(ATDPlayerState* Target)
{
	if (!IsPartyLeader() || Target == nullptr || Target == GetOwnerPlayerState())
	{
		return;
	}

	UTDPartyComponent* TargetParty = Target->GetPartyComponent();
	if (TargetParty == nullptr || TargetParty->PartyId != PartyId)
	{
		return;
	}

	TargetParty->SetPartyState(PartyId, /*bNewIsLeader=*/ true);
	SetPartyState(PartyId, /*bNewIsLeader=*/ false);

	UE_LOG(LogTemp, Log, TEXT("파티장 위임: %s"), *Target->GetPlayerName());
}

void UTDPartyComponent::HandleOwnerLogout()
{
	RemoveFromParty();
}

void UTDPartyComponent::RemoveFromParty()
{
	if (!PartyId.IsValid())
	{
		return;
	}

	const FGuid OldPartyId = PartyId;
	const bool bWasLeader = bIsLeader;

	SetPartyState(FGuid(), /*bNewIsLeader=*/ false);

	// 파티장이 나갔으면 남은 사람 중 한 명에게 넘긴다. 리더가 없는 파티가 되면
	// 초대도 추방도 못 하는 상태로 굳는다.
	if (bWasLeader)
	{
		const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		if (GameState != nullptr)
		{
			for (APlayerState* PlayerState : GameState->PlayerArray)
			{
				ATDPlayerState* TDPlayerState = Cast<ATDPlayerState>(PlayerState);
				UTDPartyComponent* OtherParty =
					TDPlayerState ? TDPlayerState->GetPartyComponent() : nullptr;

				if (OtherParty != nullptr && OtherParty->PartyId == OldPartyId)
				{
					OtherParty->SetPartyState(OldPartyId, /*bNewIsLeader=*/ true);

					UE_LOG(LogTemp, Log, TEXT("파티장 자동 위임: %s"),
						*TDPlayerState->GetPlayerName());
					break;
				}
			}
		}
	}

	DissolveIfAlone(OldPartyId);
}

void UTDPartyComponent::DissolveIfAlone(const FGuid& TargetPartyId)
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (GameState == nullptr || !TargetPartyId.IsValid())
	{
		return;
	}

	UTDPartyComponent* LastMember = nullptr;
	int32 Count = 0;

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		ATDPlayerState* TDPlayerState = Cast<ATDPlayerState>(PlayerState);
		UTDPartyComponent* OtherParty =
			TDPlayerState ? TDPlayerState->GetPartyComponent() : nullptr;

		if (OtherParty != nullptr && OtherParty->PartyId == TargetPartyId)
		{
			LastMember = OtherParty;
			++Count;
		}
	}

	// 혼자 남은 파티를 유지하면 "파티 중" 표시가 계속 뜨고,
	// 경험치 보너스도 1명짜리 파티라는 예외를 다뤄야 한다.
	if (Count == 1 && LastMember != nullptr)
	{
		LastMember->SetPartyState(FGuid(), /*bNewIsLeader=*/ false);

		UE_LOG(LogTemp, Log, TEXT("파티 해체: 혼자 남았다."));
	}
}

void UTDPartyComponent::SetPartyState(const FGuid& NewPartyId, bool bNewIsLeader)
{
	if (PartyId == NewPartyId && bIsLeader == bNewIsLeader)
	{
		return;
	}

	PartyId = NewPartyId;
	bIsLeader = bNewIsLeader;

	// 서버에서는 OnRep 이 불리지 않으므로 직접 알린다.
	OnPartyChanged.Broadcast();

	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}

	// 파티원 목록이 바뀌었으므로 나머지 파티원의 UI 도 갱신돼야 한다.
	// 각자의 PartyId 는 그대로라 OnRep 이 안 불리기 때문에 여기서 알려준다.
	for (ATDPlayerState* Member : GetPartyMembers())
	{
		UTDPartyComponent* MemberParty = Member ? Member->GetPartyComponent() : nullptr;
		if (MemberParty != nullptr && MemberParty != this)
		{
			MemberParty->OnPartyChanged.Broadcast();
		}
	}
}

void UTDPartyComponent::OnRep_PartyState()
{
	OnPartyChanged.Broadcast();
}
