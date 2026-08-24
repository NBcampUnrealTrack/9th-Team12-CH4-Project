#include "Character/TDCharacterBase.h"

#include "Core/TDGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Stats/TDProgressionComponent.h"
#include "Stats/TDStatComponent.h"

void ATDCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	BindToStatComponent();
}

void ATDCharacterBase::BindToStatComponent()
{
	if (bBoundToStatComponent)
	{
		return;
	}

	UTDStatComponent* StatComponent = GetStatComponent();
	if (StatComponent == nullptr)
	{
		// 플레이어는 PlayerState 복제 전이라 아직 없을 수 있다. 오류가 아니다.
		return;
	}

	StatComponent->OnStatsChanged.AddDynamic(this, &ATDCharacterBase::HandleStatsChanged);
	bBoundToStatComponent = true;

	// 구독 시점 이전에 이미 등록된 모디파이어가 있을 수 있으므로 한 번 반영하고 시작한다.
	HandleStatsChanged();
}

void ATDCharacterBase::SetGenericTeamId(const FGenericTeamId& NewTeamId)
{
	TeamId = NewTeamId;
}

void ATDCharacterBase::HandleDeath()
{
	// 체력이 0 아래로 여러 번 깎이거나 서버·클라 양쪽에서 불릴 수 있으므로 한 번만 통과시킨다.
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	OnDeath.Broadcast();
}

void ATDCharacterBase::HandleStatsChanged()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		// 스탯 컴포넌트가 아직 없으면 엔진 기본값을 그대로 쓴다.
		Movement->MaxWalkSpeed = GetStat(TDTags::Stat_Utility_MoveSpeed, Movement->MaxWalkSpeed);
	}
}

UTDStatComponent* ATDCharacterBase::GetStatComponent() const
{
	// 플레이어는 PlayerState 에 있으므로 ATDPlayerCharacter 가 이 함수를 재정의한다.
	return FindComponentByClass<UTDStatComponent>();
}

UAbilitySystemComponent* ATDCharacterBase::GetAbilitySystemComponent() const
{
	// 기본은 없다. 몬스터는 자기 것을, 플레이어는 PlayerState 것을 돌려주도록 재정의한다.
	return nullptr;
}

UTDProgressionComponent* ATDCharacterBase::GetProgressionComponent() const
{
	// 몬스터는 성장 컴포넌트를 갖지 않으므로 여기서 nullptr 이 나온다.
	// 플레이어는 ATDPlayerCharacter 가 PlayerState 쪽을 가리키도록 재정의한다.
	return FindComponentByClass<UTDProgressionComponent>();
}

float ATDCharacterBase::GetStat(FGameplayTag Stat, float DefaultValue) const
{
	const UTDStatComponent* StatComponent = GetStatComponent();
	return StatComponent ? StatComponent->GetStat(Stat) : DefaultValue;
}
