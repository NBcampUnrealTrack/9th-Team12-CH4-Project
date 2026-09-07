#include "Character/TDEnemyBase.h"

#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Components/CapsuleComponent.h"                    
#include "Core/TDGameplayTags.h"
#include "Data/TDMonsterRow.h"
#include "Engine/DataTable.h"
#include "GameFramework/CharacterMovementComponent.h"      
#include "Stats/TDProgressionComponent.h"                   
#include "Stats/TDStatComponent.h"
#include "Party/TDPartyComponent.h"
#include "Player/TDPlayerState.h"
#include "Quest/TDQuestComponent.h"

ATDEnemyBase::ATDEnemyBase()
{
	StatComponent = CreateDefaultSubobject<UTDStatComponent>(TEXT("StatComponent"));

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Minimal: 어트리뷰트(체력)는 전원에게 가지만 GameplayEffect 상세는 아무에게도 안 간다.
	// 몬스터가 어떤 버프를 몇 초 남기고 있는지는 클라이언트가 알 필요가 없다.
	// 태그와 이펙트 큐는 여전히 복제되므로 화면 연출에는 지장이 없다.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UTDAttributeSet>(TEXT("AttributeSet"));

	// 레벨에 미리 배치한 몬스터와 스포너가 만든 몬스터 모두 AI 가 자동으로 빙의한다.
	// 기본값(Disabled)으로 두면 배치한 몬스터가 아무것도 하지 않아 AI 담당이 매번 BP 에서 켜야 한다.
	// AIControllerClass 는 여기서 지정하지 않는다 — C++ 에서 블루프린트를 참조하면 경로가 코드에 박힌다.
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

UTDStatComponent* ATDEnemyBase::GetStatComponent() const
{
	return StatComponent;
}

UAbilitySystemComponent* ATDEnemyBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ATDEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	// 몬스터는 Owner 와 Avatar 가 모두 자기 자신이다. 플레이어처럼 PlayerState 복제를
	// 기다릴 필요가 없어 서버·클라 양쪽에서 여기 한 번이면 된다.
	if (AbilitySystemComponent != nullptr)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	// 레벨에 배치된 몬스터는 에디터에서 지정한 MonsterId/Level 로 시작한다.
	// 스포너가 만드는 몬스터는 InitializeFromDefinition 이 먼저 불려 값이 이미 채워져 있다.
	ApplyDefinition();
}

void ATDEnemyBase::UpdateVitalAttributes()
{
	if (!HasAuthority() || StatComponent == nullptr || AbilitySystemComponent == nullptr)
	{
		return;
	}

	const float NewMaxHealth = StatComponent->GetStat(TDTags::Stat_Resource_Health_Max);
	const float NewMaxMana = StatComponent->GetStat(TDTags::Stat_Resource_Mana_Max);

	AbilitySystemComponent->SetNumericAttributeBase(UTDAttributeSet::GetMaxHealthAttribute(), NewMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UTDAttributeSet::GetMaxManaAttribute(), NewMaxMana);

	// 몬스터는 세이브가 없으므로 항상 최대치로 시작한다.
	AbilitySystemComponent->SetNumericAttributeBase(UTDAttributeSet::GetHealthAttribute(), NewMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UTDAttributeSet::GetManaAttribute(), NewMaxMana);
}

void ATDEnemyBase::InitializeFromDefinition(FName InMonsterId, int32 InLevel)
{
	if (!HasAuthority())
	{
		return;
	}

	MonsterId = InMonsterId;
	Level = FMath::Max(1, InLevel);

	ApplyDefinition();
}

void ATDEnemyBase::ApplyDefinition()
{
	// 스탯 초기화는 서버가 정한다. 클라이언트는 복제된 결과만 받는다.
	if (!HasAuthority())
	{
		return;
	}

	if (MonsterTable == nullptr || MonsterId.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: MonsterTable 또는 MonsterId 가 지정되지 않아 스탯을 초기화하지 못했다."),
			*GetName());
		return;
	}

	const FTDMonsterRow* Row = MonsterTable->FindRow<FTDMonsterRow>(MonsterId, TEXT("ATDEnemyBase::ApplyDefinition"));
	if (Row == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: DT_MonsterDefinition 에 '%s' 행이 없다."),
			*GetName(), *MonsterId.ToString());
		return;
	}

	// 이전 설정이 남아 있을 수 있다. 재초기화(스포너 재활용 등)를 고려해 비우고 시작한다.
	StatComponent->ClearSources();

	// 테이블 값은 모디파이어가 아니라 기본값으로 들어간다.
	const float LevelAsFloat = static_cast<float>(Level);

	StatComponent->SetBaseValue(TDTags::Stat_Resource_Health_Max, Row->BaseHealth.GetValueAtLevel(LevelAsFloat));
	StatComponent->SetBaseValue(TDTags::Stat_Defense_Armor, Row->BaseDefense.GetValueAtLevel(LevelAsFloat));

	// 공격 타입을 지정하지 않은 몬스터는 물리로 취급한다.
	// 상위 태그(Stat.Offense.Damage)에 넣으면 기본값이 계층을 타지 않아 하위 계산에 반영되지 않는다.
	const FGameplayTag DamageTag = Row->DamageType.IsValid()
		? Row->DamageType
		: TDTags::Stat_Offense_Damage_Physical;

	StatComponent->SetBaseValue(DamageTag, Row->BaseDamage.GetValueAtLevel(LevelAsFloat));

	// 보상도 같은 행에서 레벨 스케일로 읽어둔다. 죽는 시점엔 테이블을 다시 열지 않는다.
	ExpReward = FMath::RoundToInt(Row->ExpReward.GetValueAtLevel(LevelAsFloat));
	
	//  어트리뷰트에 스탯을 옮겨서 클라이언트가 볼 수 있도록 한다.
	UpdateVitalAttributes();
}

void ATDEnemyBase::HandleDeath()
{
	// 부모가 bIsDead 를 세우고 OnDeath 를 브로드캐스트한다. AI 정지는 그 구독자 몫이다.
	Super::HandleDeath();

	// 시체는 길을 막지도, 맞지도 않는다. 히트 판정(ECC_Pawn 질의)에서도 이걸로 빠진다.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	// 파괴는 서버 권한. 복제로 클라이언트에서도 함께 사라진다.
	// TODO(드랍): GoldMin/Max 와 DropTableId 는 드랍 액터(W3)와 함께 붙인다.
	if (HasAuthority())
	{
		SetLifeSpan(CorpseLifetime);
	}
}

void ATDEnemyBase::GrantRewards(
	ATDCharacterBase* Killer)
{
	if (!HasAuthority()
		|| bRewardsGranted
		|| Killer == nullptr)
	{
		return;
	}

	bRewardsGranted = true;

	ATDPlayerState* KillerPlayerState = nullptr;

	/**
	 * 일반 플레이어 캐릭터인 경우.
	 */
	KillerPlayerState =
		Killer->GetPlayerState<ATDPlayerState>();

	/**
	 * 향후 펫·소환수인 경우:
	 * 소환수의 Controller가 플레이어 컨트롤러라면 그 PlayerState를 사용한다.
	 */
	if (KillerPlayerState == nullptr)
	{
		if (AController* KillerController =
			Killer->GetController())
		{
			KillerPlayerState =
				KillerController
					->GetPlayerState<ATDPlayerState>();
		}
	}

	/**
	 * 소환수 Owner가 플레이어 캐릭터인 경우도 확인한다.
	 */
	if (KillerPlayerState == nullptr)
	{
		AActor* OwnerActor = Killer->GetOwner();

		if (ACharacter* OwnerCharacter =
			Cast<ACharacter>(OwnerActor))
		{
			KillerPlayerState =
				OwnerCharacter
					->GetPlayerState<ATDPlayerState>();
		}
	}

	if (KillerPlayerState == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("몬스터 '%s': 처치자 PlayerState를 찾지 못했다."),
			*MonsterId.ToString());
		return;
	}

	/**
	 * 경험치는 기존 파티 분배 경로를 사용한다.
	 */
	if (UTDPartyComponent* Party =
		KillerPlayerState->GetPartyComponent())
	{
		Party->AwardKillExp(ExpReward);
	}

	/**
	 * 퀘스트는 같은 존의 살아 있는 파티원만 받는다.
	 * 거리는 검사하지 않으며 실제 공격 참여 여부도 검사하지 않는다.
	 */
	const FGameplayTag KillZone =
		KillerPlayerState->GetCurrentZoneId();

	UTDPartyComponent* KillerParty =
		KillerPlayerState->GetPartyComponent();

	const TArray<ATDPlayerState*> Members =
		KillerParty
			? KillerParty->GetPartyMembers()
			: TArray<ATDPlayerState*>{
				KillerPlayerState
			};

	for (ATDPlayerState* Member : Members)
	{
		if (Member == nullptr
			|| Member->GetCurrentZoneId()
				!= KillZone)
		{
			continue;
		}

		const ATDCharacterBase* MemberCharacter =
			Cast<ATDCharacterBase>(
				Member->GetPawn());

		/**
		 * 접속은 했지만 캐릭터가 없거나,
		 * 사망 상태인 파티원은 진행도를 받지 않는다.
		 */
		if (MemberCharacter == nullptr
			|| MemberCharacter->IsDead())
		{
			continue;
		}

		if (UTDQuestComponent* Quest =
			Member->GetQuestComponent())
		{
			Quest->ReportMonsterKilled(
				MonsterId);
		}
	}
}