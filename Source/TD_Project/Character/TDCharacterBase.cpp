#include "Character/TDCharacterBase.h"
#include "Combat/TDCombatComponent.h"
#include "Combat/TDCombatStatics.h"
#include "Core/TDGameplayTags.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
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

	// 재생은 서버가 굴린다. 어트리뷰트를 바꾸는 것은 서버 권한이고,
	// 결과인 Health/Mana 는 복제로 클라이언트에 전달된다.
	//
	// 재생량이 0 인 캐릭터(지금은 대부분의 몬스터)도 타이머는 돈다. 스탯은 장비·버프로
	// 언제든 붙을 수 있어 시작 시점의 값으로 켜고 끌 수 없기 때문이다 —
	// 값이 0 이면 TickRegen 안에서 즉시 빠져나간다.
	if (HasAuthority() && RegenIntervalSeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(RegenTimerHandle, this,
			&ATDCharacterBase::TickRegen, RegenIntervalSeconds, /*bLoop=*/true);
	}
}

void ATDCharacterBase::TickRegen()
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	// 스탯은 "초당" 값이다. 주기를 바꿔도 총 회복량이 같도록 간격만큼 곱한다.
	const float HealthPerSecond = GetStat(TDTags::Stat_Resource_Health_Regen);
	const float ManaPerSecond = GetStat(TDTags::Stat_Resource_Mana_Regen);

	if (HealthPerSecond > 0.f)
	{
		// 가득 찼으면 RestoreHealth 가 false 를 돌려주고 아무 일도 하지 않는다.
		UTDCombatStatics::RestoreHealth(this, HealthPerSecond * RegenIntervalSeconds);
	}

	if (ManaPerSecond > 0.f)
	{
		UTDCombatStatics::RestoreMana(this, ManaPerSecond * RegenIntervalSeconds);
	}
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

void ATDCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 남의 사망도 보여야 한다 — 파티원 상태 표시, 시체 공격 방지, 사망 애니메이션.
	DOREPLIFETIME(ATDCharacterBase, bIsDead);
}

void ATDCharacterBase::HandleDeath()
{
	// 체력이 0 아래로 여러 번 깎일 수 있으므로 한 번만 통과시킨다.
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	// 서버에서는 OnRep 이 불리지 않으므로 직접 알린다.
	OnDeath.Broadcast();
	ForceNetUpdate();
}

void ATDCharacterBase::HandleRespawn()
{
	if (!bIsDead)
	{
		return;
	}

	bIsDead = false;

	OnRespawn.Broadcast();
	ForceNetUpdate();
}

void ATDCharacterBase::OnRep_IsDead()
{
	// 값에 따라 갈라 부른다. 사망과 부활을 하나의 델리게이트로 합치면
	// 구독하는 쪽이 매번 IsDead() 를 다시 확인해야 한다.
	if (bIsDead)
	{
		OnDeath.Broadcast();
	}
	else
	{
		OnRespawn.Broadcast();
	}
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

void ATDCharacterBase::ReceiveHit(AActor* Attacker, float Damage, bool bCritical)
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	if (CanBeStaggered())
	{
		StaggerEndTime = GetWorld()->GetTimeSeconds() + HitStaggerDuration;

		// 휘두르던 중이면 그 스윙은 무효다. "맞으면 멈춘다"의 핵심이 여기다.
		if (CombatComponent != nullptr)
		{
			CombatComponent->CancelAttack();
		}
	}

	// 서버 구독자(AI 어그로) 먼저, 그다음 전 머신 연출. 서로 기다리지 않는다.
	OnDamagedServer.Broadcast(Attacker, Damage, bCritical);
	MulticastOnDamaged(Attacker, Damage, bCritical);
}

void ATDCharacterBase::MulticastOnDamaged_Implementation(AActor* Attacker, float Damage, bool bCritical)
{
	OnDamaged.Broadcast(Attacker, Damage, bCritical);
}

bool ATDCharacterBase::IsStaggered() const
{
	return GetWorld() != nullptr && GetWorld()->GetTimeSeconds() < StaggerEndTime;
}

void ATDCharacterBase::SetInvulnerable(float Duration)
{
	if (!HasAuthority() || Duration <= 0.f || GetWorld() == nullptr)
	{
		return;
	}

	// 이미 걸린 무적이 더 길면 그대로 둔다. 덮어쓰면 짧은 무적이 긴 무적을 끊는다.
	const float EndTime = GetWorld()->GetTimeSeconds() + Duration;
	InvulnerableEndTime = FMath::Max(InvulnerableEndTime, EndTime);
}

bool ATDCharacterBase::IsInvulnerable() const
{
	return GetWorld() != nullptr && GetWorld()->GetTimeSeconds() < InvulnerableEndTime;
}
