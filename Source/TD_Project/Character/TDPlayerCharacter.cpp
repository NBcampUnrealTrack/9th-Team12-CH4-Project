#include "Character/TDPlayerCharacter.h"

#include "AbilitySystemComponent.h"
#include "Player/TDPlayerState.h"

UTDStatComponent* ATDPlayerCharacter::GetStatComponent() const
{
	// 클라이언트에서는 PlayerState 복제가 끝나기 전까지 nullptr 이 나온다.
	// 오류가 아니라 정상 상태이므로, 호출하는 쪽이 결과를 확인해야 한다.
	const ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	return TDPlayerState ? TDPlayerState->GetStatComponent() : nullptr;
}

UTDProgressionComponent* ATDPlayerCharacter::GetProgressionComponent() const
{
	// 레벨과 포인트 분배도 PlayerState 에 있다. 리스폰으로 이 액터가 파괴돼도 남아야 하기 때문.
	const ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	return TDPlayerState ? TDPlayerState->GetProgressionComponent() : nullptr;
}

UAbilitySystemComponent* ATDPlayerCharacter::GetAbilitySystemComponent() const
{
	const ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	return TDPlayerState ? TDPlayerState->GetAbilitySystemComponent() : nullptr;
}

void ATDPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버 경로. 컨트롤러가 빙의하면서 PlayerState 연결이 끝난 시점이다.
	InitAbilityActorInfo();
	BindToStatComponent();
}

void ATDPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라이언트 경로. 이제야 PlayerState 와 그 안의 컴포넌트들에 접근할 수 있다.
	InitAbilityActorInfo();
	BindToStatComponent();
}

void ATDPlayerCharacter::InitAbilityActorInfo()
{
	ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	if (TDPlayerState == nullptr)
	{
		return;
	}

	UAbilitySystemComponent* ASC = TDPlayerState->GetAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	// Owner 는 ASC 를 소유한 액터(PlayerState), Avatar 는 월드에서 그것을 대신하는 액터(캐릭터).
	// 둘을 나누는 이유는 수명이 다르기 때문이다 — 캐릭터는 죽으면 사라지지만 PlayerState 는 남는다.
	ASC->InitAbilityActorInfo(TDPlayerState, this);
}
