#include "Player/TDPlayerState.h"

#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Character/TDCharacterBase.h"
#include "Core/TDGameplayTags.h"
#include "Engine/World.h"
#include "Game/TDGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Items/TDInventoryComponent.h"
#include "Items/TDItemUseComponent.h"
#include "Net/UnrealNetwork.h"
#include "Items/TDQuickSlotComponent.h"
#include "Party/TDPartyComponent.h"
#include "Stats/TDProgressionComponent.h"
#include "Stats/TDStatComponent.h"

ATDPlayerState::ATDPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Mixed: 어트리뷰트는 전원에게, GameplayEffect 상세는 소유 클라에게만 보낸다.
	// 남의 버프 목록까지 받을 이유가 없다. 몬스터는 Minimal 을 쓴다.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UTDAttributeSet>(TEXT("AttributeSet"));

	StatComponent = CreateDefaultSubobject<UTDStatComponent>(TEXT("StatComponent"));
	ProgressionComponent = CreateDefaultSubobject<UTDProgressionComponent>(TEXT("ProgressionComponent"));
	InventoryComponent = CreateDefaultSubobject<UTDInventoryComponent>(TEXT("InventoryComponent"));
	ItemUseComponent = CreateDefaultSubobject<UTDItemUseComponent>(TEXT("ItemUseComponent"));
	PartyComponent = CreateDefaultSubobject<UTDPartyComponent>(TEXT("PartyComponent"));
	QuickSlotComponent = CreateDefaultSubobject<UTDQuickSlotComponent>(TEXT("QuickSlotComponent"));

	// PlayerState 의 기본 갱신 빈도는 1Hz 다. 그대로 두면 여기 실린 값이 초당 한 번씩만
	// 클라이언트로 가서, 체력바가 1초에 한 칸씩 움직이는 것처럼 보인다.
	SetNetUpdateFrequency(100.f);
}

void ATDPlayerState::BeginPlay()
{
	Super::BeginPlay();

	// 전투력 계산은 서버 몫이다. 클라이언트는 복제된 값을 받기만 한다.
	if (HasAuthority() && StatComponent != nullptr)
	{
		StatComponent->OnStatsChanged.AddDynamic(this, &ATDPlayerState::HandleStatsChanged);

		// 레벨업하면 체력·마나를 가득 채운다. 최대치가 오르는 것과 별개인 게임 규칙이라
		// UpdateVitalAttributes 가 아니라 여기서 따로 처리한다.
		if (ProgressionComponent != nullptr)
		{
			ProgressionComponent->OnLevelUp.AddDynamic(this, &ATDPlayerState::HandleLevelUp);
		}

		// 구독은 늦었다. 컴포넌트들의 BeginPlay 는 위의 Super::BeginPlay() 안에서 이미 끝났고,
		// 그때 나간 OnStatsChanged 는 아직 구독 전이라 받지 못했다.
		// 스탯 자체는 이미 최종값이므로, 놓친 알림 대신 지금 상태를 한 번 옮겨 적는다.
		HandleStatsChanged();
	}
}

void ATDPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 인벤토리와 달리 조건을 걸지 않는다. 파티원이나 주변 플레이어에게도 보여야 하기 때문.
	DOREPLIFETIME(ATDPlayerState, ReplicatedCombatPower);

	// 직업은 다른 플레이어의 스프라이트를 정하는 값이라 역시 전원에게 보낸다.
	DOREPLIFETIME(ATDPlayerState, CharacterClassId);

	// 캐릭터 목록은 본인만 본다. 남이 어떤 캐릭터를 가졌는지 알 이유가 없다.
	DOREPLIFETIME_CONDITION(ATDPlayerState, CharacterSlots, COND_OwnerOnly);

	// 스탯창은 자기 것만 본다. 남에게 보여야 하는 것은 전투력뿐이고 그쪽은 위에서 전원에게 간다.
	DOREPLIFETIME_CONDITION(ATDPlayerState, ReplicatedStats, COND_OwnerOnly);

	// 선택 여부는 소유자만 알면 되지만, 나중에 "선택 중" 상태를 남에게 보여줄 수 있으므로
	// 조건을 걸지 않는다. bool 하나라 비용이 없다.
	DOREPLIFETIME(ATDPlayerState, bCharacterSelected);

	// 파티 UI 가 파티원이 어느 존에 있는지 표시해야 하므로 조건을 걸지 않는다.
	// FGameplayTag 는 사실상 인덱스 하나라 30명 전원에게 보내도 비용이 없고,
	// 같은 존에 있으면 어차피 그 사람의 캐릭터가 화면에 보인다.
	DOREPLIFETIME(ATDPlayerState, CurrentZoneId);
}

// ── 존 ────────────────────────────────────────────────────

bool ATDPlayerState::SetCurrentZoneId(FGameplayTag NewZoneId)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("SetCurrentZoneId 는 서버에서만 호출해야 한다."));
		return false;
	}

	if (CurrentZoneId == NewZoneId)
	{
		return false;
	}

	CurrentZoneId = NewZoneId;

	// 서버에서는 OnRep 이 불리지 않으므로 직접 알린다.
	OnZoneChanged.Broadcast(CurrentZoneId);
	ForceNetUpdate();

	return true;
}

void ATDPlayerState::OnRep_CurrentZoneId()
{
	OnZoneChanged.Broadcast(CurrentZoneId);
}

void ATDPlayerState::SetCharacterClassId(FName NewClassId)
{
	if (!HasAuthority() || CharacterClassId == NewClassId)
	{
		return;
	}

	CharacterClassId = NewClassId;

	// 성장 테이블도 같은 식별자를 쓰므로 함께 맞춘다.
	// 이 호출로 레벨 성장 모디파이어가 새 직업 기준으로 다시 만들어진다.
	if (ProgressionComponent != nullptr)
	{
		ProgressionComponent->SetClassId(NewClassId);
	}

	OnCharacterClassChanged.Broadcast(CharacterClassId);
	ForceNetUpdate();
}

void ATDPlayerState::OnRep_CharacterClassId()
{
	OnCharacterClassChanged.Broadcast(CharacterClassId);
}

UAbilitySystemComponent* ATDPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// ── 캐릭터 선택 ───────────────────────────────────────────

void ATDPlayerState::SetCharacterSlots(TArray<FTDCharacterSummary> InSlots)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("SetCharacterSlots 는 서버에서만 호출해야 한다."));
		return;
	}

	CharacterSlots = MoveTemp(InSlots);

	// 서버에서는 OnRep 이 불리지 않으므로 직접 알린다.
	OnCharacterSlotsChanged.Broadcast();
	ForceNetUpdate();
}

void ATDPlayerState::OnRep_CharacterSlots()
{
	OnCharacterSlotsChanged.Broadcast();
}

void ATDPlayerState::ServerSelectCharacter_Implementation(int32 SlotIndex)
{
	SelectCharacter(SlotIndex);
}

bool ATDPlayerState::SelectCharacter(int32 SlotIndex)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("SelectCharacter: 서버 권한이 없다."));
		return false;
	}

	// 한 번 고르고 나면 바꿀 수 없다. 캐릭터를 갈아타려면 접속을 다시 해야 한다.
	// 인게임 중에 허용하면 인벤토리·스탯을 통째로 교체하는 경로가 필요해진다.
	if (bCharacterSelected)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SelectCharacter: 이미 %d번 캐릭터를 선택했다."), SelectedSlotIndex);
		return false;
	}

	if (!CharacterSlots.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SelectCharacter: 슬롯 %d 이 없다. (보유 %d개)"), SlotIndex, CharacterSlots.Num());
		return false;
	}

	const FTDCharacterSummary& Chosen = CharacterSlots[SlotIndex];

	SelectedSlotIndex = SlotIndex;
	bCharacterSelected = true;

	// 캐릭터 이름을 PlayerState 의 표시 이름으로 삼는다.
	//
	// CharacterSlots 는 COND_OwnerOnly 라 남의 캐릭터 이름을 알 방법이 없다.
	// PlayerName 은 엔진이 이미 전원에게 복제하므로, 파티 UI·이름표·채팅이
	// 별도 복제 없이 GetPlayerName() 하나로 해결된다.
	SetPlayerName(Chosen.CharacterName);

	// 직업을 정하면 성장 모디파이어가 그 직업 기준으로 다시 만들어진다.
	SetCharacterClassId(Chosen.ClassId);

	if (ProgressionComponent != nullptr)
	{
		ProgressionComponent->SetLevel(Chosen.Level);
	}

	// 캐릭터가 정해진 지금이 진짜 초기화 시점이다.
	// 접속 직후에도 어트리뷰트가 한 번 채워지지만, 그때는 어느 캐릭터인지 몰라
	// 레벨 1 기준으로 들어가 있다. 그대로 두면 25레벨 캐릭터가 반피로 시작한다.
	//
	// 세이브가 붙으면 SetSavedVitalRatios() 를 먼저 부르고 여기로 오면 된다 —
	// 저장된 비율이 이 재초기화에서 반영된다.
	bVitalsInitialized = false;
	UpdateVitalAttributes();

	UE_LOG(LogTemp, Log,
		TEXT("캐릭터 선택: %s (%s, Lv.%d) — 슬롯 %d"),
		*Chosen.CharacterName, *Chosen.ClassId.ToString(), Chosen.Level, SlotIndex);

	OnCharacterSelected.Broadcast();
	ForceNetUpdate();

	// 스폰은 GameMode 권한이다. 여기서 Pawn 을 직접 만들지 않는다 —
	// 만들면 스폰 지점 결정과 리스폰 경로가 두 군데로 갈라진다.
	if (ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr)
	{
		GameMode->HandleCharacterSelected(GetPlayerController());
	}

	return true;
}

void ATDPlayerState::OnRep_CharacterSelected()
{
	OnCharacterSelected.Broadcast();
}

void ATDPlayerState::SetSavedVitalRatios(float InHealthRatio, float InManaRatio)
{
	SavedHealthRatio = FMath::Clamp(InHealthRatio, 0.f, 1.f);
	SavedManaRatio = FMath::Clamp(InManaRatio, 0.f, 1.f);
}

void ATDPlayerState::HandleLevelUp(int32 NewLevel, int32 PreviousLevel)
{
	if (!HasAuthority() || AbilitySystemComponent == nullptr)
	{
		return;
	}

	// 시체는 채우지 않는다. 그러지 않으면 죽은 채로 경험치만 받아도 되살아나,
	// 레벨업이 부활 수단이 되어버린다. 부활은 부활 로직이 담당해야 한다.
	//
	// 최대치가 오른 것은 HandleStatsChanged 가 이미 반영했으므로 여기서 빠져나가도 문제없다.
	const ATDCharacterBase* Character = Cast<ATDCharacterBase>(GetPawn());
	if (Character != nullptr && Character->IsDead())
	{
		return;
	}

	// 레벨업 시 체력·마나를 가득 채운다. 최대치 갱신은 이미 HandleStatsChanged 가
	// 처리했으므로, 여기서는 현재값만 최대치로 끌어올리면 된다.
	AbilitySystemComponent->SetNumericAttributeBase(
		UTDAttributeSet::GetHealthAttribute(),
		AbilitySystemComponent->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute()));

	AbilitySystemComponent->SetNumericAttributeBase(
		UTDAttributeSet::GetManaAttribute(),
		AbilitySystemComponent->GetNumericAttribute(UTDAttributeSet::GetMaxManaAttribute()));
}

void ATDPlayerState::HandleStatsChanged()
{
	UpdateCombatPower();
	UpdateReplicatedStats();
	UpdateVitalAttributes();
}

float ATDPlayerState::GetReplicatedStat(FGameplayTag Stat) const
{
	const FTDStatSnapshot* Found = ReplicatedStats.FindByPredicate(
		[Stat](const FTDStatSnapshot& Snapshot) { return Snapshot.Stat == Stat; });

	return Found ? Found->Value : 0.f;
}

void ATDPlayerState::UpdateReplicatedStats()
{
	if (!HasAuthority() || StatComponent == nullptr)
	{
		return;
	}

	// 보낼 목록은 DT_StatDefinition 이 정한다. 테이블에 행을 추가하면 여기도 자동으로 늘어난다.
	const TArray<FGameplayTag> DefinedStats = StatComponent->GetDefinedStats();

	TArray<FTDStatSnapshot> NewSnapshot;
	NewSnapshot.Reserve(DefinedStats.Num());

	for (const FGameplayTag& Stat : DefinedStats)
	{
		NewSnapshot.Emplace(Stat, StatComponent->GetStat(Stat));
	}

	// 값이 그대로면 복제하지 않는다. 이동 속도만 바뀌어도 15개를 다시 보내는 것을 막는다.
	if (ReplicatedStats == NewSnapshot)
	{
		return;
	}

	ReplicatedStats = MoveTemp(NewSnapshot);

	// 서버에서는 OnRep 이 불리지 않으므로 여기서 직접 알린다.
	OnStatsReplicated.Broadcast();
}

void ATDPlayerState::OnRep_ReplicatedStats()
{
	OnStatsReplicated.Broadcast();
}

void ATDPlayerState::UpdateVitalAttributes()
{
	if (!HasAuthority() || StatComponent == nullptr || AbilitySystemComponent == nullptr)
	{
		return;
	}

	// 최대치는 스탯 컴포넌트가 계산한 결과를 그대로 적는다.
	// GAS 모디파이어를 쓰지 않는 이유는 D2 규칙을 표현할 수 없기 때문이다(D8).
	const float NewMaxHealth = StatComponent->GetStat(TDTags::Stat_Resource_Health_Max);
	const float NewMaxMana = StatComponent->GetStat(TDTags::Stat_Resource_Mana_Max);

	AbilitySystemComponent->SetNumericAttributeBase(UTDAttributeSet::GetMaxHealthAttribute(), NewMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UTDAttributeSet::GetMaxManaAttribute(), NewMaxMana);

	// 현재값은 최대치가 정해진 뒤에만 채울 수 있다.
	// 이후 최대 체력이 오르내려도 현재 체력을 건드리지 않는다 — 그건 전투 결과이지 스탯이 아니다.
	if (!bVitalsInitialized)
	{
		AbilitySystemComponent->SetNumericAttributeBase(
			UTDAttributeSet::GetHealthAttribute(), NewMaxHealth * SavedHealthRatio);

		AbilitySystemComponent->SetNumericAttributeBase(
			UTDAttributeSet::GetManaAttribute(), NewMaxMana * SavedManaRatio);

		bVitalsInitialized = true;
	}
}

void ATDPlayerState::UpdateCombatPower()
{
	if (!HasAuthority() || StatComponent == nullptr)
	{
		return;
	}

	const int32 NewCombatPower = FMath::RoundToInt(StatComponent->GetCombatPower());
	if (ReplicatedCombatPower == NewCombatPower)
	{
		return;
	}

	ReplicatedCombatPower = NewCombatPower;

	// 서버에서는 OnRep 이 불리지 않으므로 여기서 직접 알린다.
	OnCombatPowerChanged.Broadcast(ReplicatedCombatPower);
}

void ATDPlayerState::OnRep_CombatPower()
{
	OnCombatPowerChanged.Broadcast(ReplicatedCombatPower);
}
