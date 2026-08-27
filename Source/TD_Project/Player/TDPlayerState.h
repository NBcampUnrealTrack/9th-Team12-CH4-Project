#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Save/TDPlayerSaveData.h"
#include "Stats/TDStatTypes.h"
#include "TDPlayerState.generated.h"

class UAbilitySystemComponent;
class UTDAttributeSet;
class UTDInventoryComponent;
class UTDItemUseComponent;
class UTDProgressionComponent;
class UTDStatComponent;

/** 전투력이 바뀌었을 때. 본인과 다른 플레이어 양쪽에서 불린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOnCombatPowerChanged, int32, NewCombatPower);

/** 직업이 정해지거나 바뀌었을 때. 캐릭터가 이걸 받아 스프라이트를 교체한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOnCharacterClassChanged, FName, NewClassId);

/** 캐릭터 목록이 도착했을 때. 선택 화면이 이걸 받아 프리뷰를 세운다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnCharacterSlotsChanged);

/** 캐릭터 선택이 확정됐을 때. 선택 화면을 닫는 신호다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnCharacterSelected);

/** 서버가 계산한 스탯이 도착했을 때. 스탯창이 이걸 받아 갱신한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnStatsReplicated);

/**
 * 플레이어의 스탯 컴포넌트가 실제로 붙는 곳.
 *
 * Pawn 이 아니라 여기에 두는 이유는 수명이다. 사망하면 Pawn 은 파괴되지만
 * PlayerState 는 접속이 유지되는 동안 살아 있으므로, 리스폰해도 장비 효과와
 * 레벨 스탯이 그대로 남는다. Pawn 에 뒀다면 죽을 때마다 전부 다시 만들어야 한다.
 */
UCLASS()
class TD_PROJECT_API ATDPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ATDPlayerState();

	/**
	 * IAbilitySystemInterface. GAS 가 액터에서 ASC 를 찾을 때 쓰는 표준 경로다.
	 * ASC 를 PlayerState 에 두는 이유는 스탯·인벤토리와 같다 — 리스폰해도 살아남아야 한다.
	 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UTDAttributeSet* GetAttributeSet() const { return AttributeSet; }

	/**
	 * 세이브에서 읽은 체력·마나 비율. 어트리뷰트를 처음 초기화할 때 쓴다.
	 *
	 * 최대치가 정해지기 전에는 적용할 수 없으므로 값만 보관해 두고,
	 * 스탯 계산이 끝나 MaxHealth 가 확정되는 시점에 곱해서 현재값을 만든다.
	 * 세이브가 없으면 1.0 이라 풀피로 시작한다.
	 */
	void SetSavedVitalRatios(float InHealthRatio, float InManaRatio);

	UTDStatComponent* GetStatComponent() const { return StatComponent; }

	UTDProgressionComponent* GetProgressionComponent() const { return ProgressionComponent; }

	UTDInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UTDItemUseComponent* GetItemUseComponent() const { return ItemUseComponent; }

	// ── 전투력 ────────────────────────────────────────────

	/**
	 * 복제된 전투력. 다른 플레이어의 값도 이걸로 읽는다.
	 *
	 * 스탯 컴포넌트는 복제되지 않으므로(계산 결과만 넘기는 구조) 남의 스탯은 알 수 없다.
	 * 전투력만 따로 실어 보내야 파티 창이나 이름표에 표시할 수 있다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Stats")
	int32 GetCombatPower() const { return ReplicatedCombatPower; }

	UPROPERTY(BlueprintAssignable, Category = "TD|Stats")
	FTDOnCombatPowerChanged OnCombatPowerChanged;

	// ── 스탯 스냅샷 ───────────────────────────────────────

	/**
	 * 서버가 계산한 스탯 최종값. **스탯창은 이걸 읽어야 한다.**
	 *
	 * UTDStatComponent 는 복제되지 않으므로 클라이언트에서 GetStat() 을 부르면
	 * DT_StatDefinition 의 기본값이 나온다 — 실제 값이 아니다.
	 *
	 * 남의 스탯창을 볼 일이 없으므로 COND_OwnerOnly 로 보낸다. 남에게 보여야 하는 것은
	 * 전투력뿐이고 그쪽은 따로 전원에게 복제된다.
	 *
	 * @return 목록에 없는 스탯이면 0. DT_StatDefinition 에 행이 있는 스탯만 담긴다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Stats")
	float GetReplicatedStat(FGameplayTag Stat) const;

	UFUNCTION(BlueprintPure, Category = "TD|Stats")
	const TArray<FTDStatSnapshot>& GetReplicatedStats() const { return ReplicatedStats; }

	/** 스탯 스냅샷이 갱신됐을 때. 스탯창은 Tick 이 아니라 이걸 구독한다. */
	UPROPERTY(BlueprintAssignable, Category = "TD|Stats")
	FTDOnStatsReplicated OnStatsReplicated;

	// ── 직업 ──────────────────────────────────────────────

	/**
	 * 선택한 직업. DT_ClassGrowth 의 ClassId 이자 DT_CharacterClass 의 RowName 이다.
	 *
	 * 다른 플레이어의 외형을 그리는 데 필요하므로 소유자에게만 보내지 않고 전원에게 복제한다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Character")
	FName GetCharacterClassId() const { return CharacterClassId; }

	/** 서버 전용. 성장 컴포넌트에도 함께 전달해 레벨 성장이 직업에 맞게 다시 계산된다. */
	void SetCharacterClassId(FName NewClassId);

	UPROPERTY(BlueprintAssignable, Category = "TD|Character")
	FTDOnCharacterClassChanged OnCharacterClassChanged;

	// ── 캐릭터 선택 (D51~D58) ─────────────────────────────

	/**
	 * 이 계정이 보유한 캐릭터 목록. 선택 화면이 읽는다.
	 *
	 * 남의 캐릭터 목록을 볼 이유가 없으므로 COND_OwnerOnly 로 복제한다.
	 * 아직 세이브가 완성되지 않아 저장하지 않으며, 지금은 TD.GiveTestCharacters 로 더미를 넣는다.
	 * TODO: 세이브 완성 후 채워넣기
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Character")
	const TArray<FTDCharacterSummary>& GetCharacterSlots() const { return CharacterSlots; }

	UPROPERTY(BlueprintAssignable, Category = "TD|Character")
	FTDOnCharacterSlotsChanged OnCharacterSlotsChanged;

	/** 서버 전용. 세이브에서 읽은 목록을 넣는다. */
	void SetCharacterSlots(TArray<FTDCharacterSummary> InSlots);

	/**
	 * 캐릭터를 골랐는 지 체크. 서버가 소유하고 클라이언트는 복제된 값을 읽기만 한다.
	 *
	 * 클라이언트가 정하게 두면 캐릭터 없이 입장할 수 있으므로 플래그를 넘기지 않는다.
	 * 선택 화면은 이 값이 false 인 동안만 떠 있으면 된다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Character")
	bool HasSelectedCharacter() const { return bCharacterSelected; }

	UPROPERTY(BlueprintAssignable, Category = "TD|Character")
	FTDOnCharacterSelected OnCharacterSelected;

	/**
	 * 선택 화면의 진입점. 슬롯 번호만 보낸다.
	 *
	 * 캐릭터 데이터를 통째로 받으면 클라이언트가 원하는 값을 만들어 보낼 수 있다.
	 * 번호만 받고 실제 내용은 서버가 자기 목록에서 읽는다.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "TD|Character")
	void ServerSelectCharacter(int32 SlotIndex);

	/** 서버 전용. 디버그 명령과 세이브 로드가 함께 쓴다. @return 실제로 선택됐으면 true. */
	bool SelectCharacter(int32 SlotIndex);

	/** 마지막으로 있던 존. 스폰 위치를 정하는 데 쓴다(D51). 비어 있으면 기본 시작 존. */
	FGameplayTag GetLastZoneId() const { return LastZoneId; }

protected:
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 스탯이 바뀔 때마다 서버에서 다시 계산한다. Tick 으로 감시하지 않는다. */
	UFUNCTION()
	void HandleStatsChanged();

private:
	void UpdateCombatPower();

	/** 정의된 스탯을 전부 계산해 스냅샷으로 옮긴다. 값이 그대로면 복제하지 않는다. */
	void UpdateReplicatedStats();

	UFUNCTION()
	void OnRep_ReplicatedStats();

	/**
	 * 스탯 15종이면 대략 120바이트다. 소유자에게만, 그리고 값이 실제로 바뀔 때만 나간다.
	 * 장착·레벨업 순간에만 갱신되므로 상시 부하는 없다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedStats)
	TArray<FTDStatSnapshot> ReplicatedStats;

	/**
	 * 스탯 계산 결과를 AttributeSet 의 최대치에 기록한다.
	 * 처음 호출될 때는 저장된 비율을 곱해 현재 체력·마나도 함께 정한다.
	 */
	void UpdateVitalAttributes();

	UFUNCTION()
	void OnRep_CombatPower();

	/**
	 * 소수점을 버리고 정수로 싣는다. 전투력은 표시용 지표라 소수점이 의미가 없고,
	 * 정수면 복제 비용도 줄어든다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_CombatPower)
	int32 ReplicatedCombatPower = 0;

	UFUNCTION()
	void OnRep_CharacterClassId();

	UPROPERTY(ReplicatedUsing = OnRep_CharacterClassId)
	FName CharacterClassId;

	UFUNCTION()
	void OnRep_CharacterSlots();

	UPROPERTY(ReplicatedUsing = OnRep_CharacterSlots)
	TArray<FTDCharacterSummary> CharacterSlots;

	UFUNCTION()
	void OnRep_CharacterSelected();

	UPROPERTY(ReplicatedUsing = OnRep_CharacterSelected)
	bool bCharacterSelected = false;

	/** 고른 슬롯 번호. 세이브를 다시 쓸 때 어느 캐릭터인지 알아야 하므로 서버가 들고 있는다. */
	int32 SelectedSlotIndex = INDEX_NONE;

	/** 세이브에서 읽어온 마지막 존. 스폰 지점 결정에만 쓰이므로 복제하지 않는다. */
	FGameplayTag LastZoneId;

	/** GAS 의 중심. 어빌리티·이펙트·어트리뷰트가 전부 여기를 거친다. */
	UPROPERTY(VisibleAnywhere, Category = "TD|Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** 현재 체력·마나. 복제되어 클라이언트가 체력바를 그릴 수 있게 한다. */
	UPROPERTY()
	TObjectPtr<UTDAttributeSet> AttributeSet;

	/** 최대치가 확정된 뒤 한 번만 현재값을 채운다. 이후에는 최대치만 갱신한다. */
	bool bVitalsInitialized = false;

	float SavedHealthRatio = 1.f;
	float SavedManaRatio = 1.f;

	/** 모디파이어를 모아 계산한다. 여기 있는 것은 전부 파생값이라 저장 대상이 아니다. */
	UPROPERTY(VisibleAnywhere, Category = "TD|Stats")
	TObjectPtr<UTDStatComponent> StatComponent;

	/** 레벨·경험치·포인트 분배를 소유한다. 저장해야 하는 값은 이쪽에 있다. */
	UPROPERTY(VisibleAnywhere, Category = "TD|Progression")
	TObjectPtr<UTDProgressionComponent> ProgressionComponent;

	/** 소지품. 사망해도 유지돼야 하므로 Pawn 이 아니라 여기 둔다. */
	UPROPERTY(VisibleAnywhere, Category = "TD|Inventory")
	TObjectPtr<UTDInventoryComponent> InventoryComponent;

	/** 장착과 아이템 사용. 장착 상태도 저장 대상이라 인벤토리와 같은 자리에 둔다. */
	UPROPERTY(VisibleAnywhere, Category = "TD|Inventory")
	TObjectPtr<UTDItemUseComponent> ItemUseComponent;
};
