#include "Character/TDCharacterBase.h"
#include "Combat/TDCombatComponent.h"
#include "Core/TDGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"
#include "Stats/TDProgressionComponent.h"
#include "Stats/TDStatComponent.h"

ATDCharacterBase::ATDCharacterBase()
{
	CombatComponent = CreateDefaultSubobject<UTDCombatComponent>(TEXT("CombatComponent"));

	// ── 2D 표현 ───────────────────────────────────────────
	// 스프라이트는 캡슐에 붙는다. 충돌은 캡슐이 전담하므로 여기서는 끈다 —
	// 켜두면 스프라이트 크기가 바뀔 때마다 판정이 따라 움직여 히트박스가 흔들린다.
	SpriteComponent = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	SpriteComponent->SetupAttachment(RootComponent);
	SpriteComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpriteComponent->SetGenerateOverlapEvents(false);

	// 스프라이트가 캐릭터 회전을 따라 돌면 옆·뒤를 볼 때 종이처럼 얇아진다.
	// 회전을 월드 기준으로 고정해 항상 같은 각도로 서 있게 한다.
	// 정확한 각도는 카메라 세팅에 달렸으므로 블루프린트에서 맞춘다.
	SpriteComponent->SetUsingAbsoluteRotation(true);

	// 애니메이션 상태 머신. 어떤 플립북을 재생할지 정해 SpriteComponent 에 밀어 넣는다.
	// AnimInstanceClass 와 RenderComponentRef 지정은 블루프린트 몫이다(플러그인에서 private).
	AnimationComponent = CreateDefaultSubobject<UPaperZDAnimationComponent>(TEXT("AnimationComponent"));
}

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
