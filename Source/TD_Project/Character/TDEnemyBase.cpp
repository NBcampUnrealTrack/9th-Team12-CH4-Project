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
#include "Components/WidgetComponent.h"
#include "UI/Combat/TDEnemyHealthBarWidget.h"
#include "Net/UnrealNetwork.h"

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
	
	SetupHealthBar();
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

void ATDEnemyBase::GrantRewards(ATDCharacterBase* Killer)
{
	if (!HasAuthority() || bRewardsGranted || Killer == nullptr)
	{
		return;
	}
	bRewardsGranted = true;

	if (UTDProgressionComponent* Progression = Killer->GetProgressionComponent())
	{
		Progression->AddExp(ExpReward);   // 레벨업 판정은 AddExp 안에서 이뤄진다
	}
}

void ATDEnemyBase::MulticastOnSense_Implementation()
{
	OnSensed.Broadcast();
}

void ATDEnemyBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATDEnemyBase, MonsterId);
}

void ATDEnemyBase::SetupHealthBar()
{
	// 데디 서버는 화면이 없다. UI 연결은 그리는 머신(클라·리슨 서버)에서만.
	if (IsRunningDedicatedServer())
	{
		return;
	}

	UWidgetComponent* WidgetComp = FindComponentByClass<UWidgetComponent>();
	if (WidgetComp == nullptr)
	{
		return;   // 위젯 컴포넌트가 없는 몬스터는 체력바 없이 동작한다 — 오류 아님
	}

	// BeginPlay 시점엔 위젯이 아직 안 만들어졌을 수 있다. 명시적으로 만들게 한다.
	WidgetComp->InitWidget();

	HealthBarWidget = Cast<UTDEnemyHealthBarWidget>(WidgetComp->GetUserWidgetObject());
	if (HealthBarWidget == nullptr || AbilitySystemComponent == nullptr)
	{
		return;
	}

	// 트리거 연결: 체력·최대체력 복제가 도착할 때마다 위젯을 갱신한다.
	// 선우님 원칙 그대로 — 서버가 계산, 복제가 전달, 클라가 표시. RPC 없음.
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UTDAttributeSet::GetHealthAttribute()).AddUObject(this, &ATDEnemyBase::HandleVitalChangedForUI);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UTDAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ATDEnemyBase::HandleVitalChangedForUI);

	// 초기값. 복제가 아직 안 왔으면 임시값(1/1)이 잠깐 보이지만, 첫 복제가 델리게이트로 바로 고쳐준다.
	HandleVitalChangedForUI(FOnAttributeChangeData());

	if (MonsterTable != nullptr && !MonsterId.IsNone())
	{
		if (const FTDMonsterRow* Row = MonsterTable->FindRow<FTDMonsterRow>(MonsterId, TEXT("HealthBar")))
		{
			HealthBarWidget->SetMonsterName(Row->DisplayName);
		}
	}
}

void ATDEnemyBase::HandleVitalChangedForUI(const FOnAttributeChangeData& Data)
{
	if (HealthBarWidget == nullptr || AbilitySystemComponent == nullptr)
	{
		return;
	}

	// 크리 여부는 어트리뷰트에 없어 일단 false — 크리 강조는 선우님과 협의 후(OnHit 편승안).
	HealthBarWidget->SetHealth(
		AbilitySystemComponent->GetNumericAttribute(UTDAttributeSet::GetHealthAttribute()),
		AbilitySystemComponent->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute()),
		false);
}