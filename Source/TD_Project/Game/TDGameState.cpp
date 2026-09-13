#include "Game/TDGameState.h"

#include "Net/UnrealNetwork.h"
#include "Character/TDBossCharacter.h"

void ATDGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATDGameState, ZoneId);
	DOREPLIFETIME(ATDGameState, ActiveBoss);
}

void ATDGameState::SetZoneId(FGameplayTag NewZoneId)
{
	if (!HasAuthority() || ZoneId == NewZoneId)
	{
		return;
	}

	ZoneId = NewZoneId;

	// 서버에서는 OnRep 이 불리지 않으므로 여기서 직접 알린다.
	OnZoneChanged.Broadcast(ZoneId);

	// 존 전환은 UI 와 스폰 판정이 곧바로 따라와야 하므로 다음 복제 주기를 기다리지 않는다.
	ForceNetUpdate();
}

void ATDGameState::OnRep_ZoneId()
{
	OnZoneChanged.Broadcast(ZoneId);
}

void ATDGameState::SetActiveBoss(ATDBossCharacter* NewBoss)
{
	if (!HasAuthority() || ActiveBoss == NewBoss)
	{
		return;
	}
	ActiveBoss = NewBoss;
	OnActiveBossChanged.Broadcast(ActiveBoss);   // 서버는 OnRep 이 안 불린다
}

void ATDGameState::OnRep_ActiveBoss()
{
	OnActiveBossChanged.Broadcast(ActiveBoss);
}
