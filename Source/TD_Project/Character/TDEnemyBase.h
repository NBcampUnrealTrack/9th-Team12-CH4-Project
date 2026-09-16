#pragma once

#include "CoreMinimal.h"
#include "Character/TDCharacterBase.h"
#include "TDEnemyBase.generated.h"

/** 적 발견 반응. 발견 모션·경계음이 구독한다. 전 머신에서 불린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnMonsterSensed);

class ATDPlayerState;
class UAbilitySystemComponent;
class UDataTable;
class UTDAttributeSet;
class UTDStatComponent;
class UWidgetComponent;
class UTDEnemyHealthBarWidget;
struct FOnAttributeChangeData;


/**
 * 몬스터의 베이스 클래스.
 *
 * 플레이어와 달리 스탯 컴포넌트를 자기가 소유한다. 몬스터는 죽으면 액터째 사라지고
 * 리스폰이 곧 재스폰이므로, 스탯이 액터보다 오래 살아남을 이유가 없다.
 *
 * 스탯은 DT_MonsterDefinition 의 행에서 읽어온다. 종류별로 1행이며 레벨 스케일은
 * FScalableFloat 이 담당하므로, 같은 행으로 1레벨 몬스터와 50레벨 몬스터를 모두 만든다.
 */
UCLASS()
class TD_PROJECT_API ATDEnemyBase : public ATDCharacterBase
{
	GENERATED_BODY()

public:
	ATDEnemyBase();

	virtual UTDStatComponent* GetStatComponent() const override;

	/**
	 * 몬스터는 ASC 를 자기가 소유한다. 죽으면 액터째 사라지므로 PlayerState 처럼
	 * 오래 사는 자리에 둘 이유가 없다. Owner 와 Avatar 가 모두 자기 자신이다.
	 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/**
	 * 정의와 레벨을 지정하고 스탯을 다시 초기화한다. 스포너가 호출한다.
	 * 서버에서만 동작하며, 배치형 몬스터는 에디터에서 설정한 값으로 BeginPlay 에 초기화된다.
	 */
	void InitializeFromDefinition(FName InMonsterId, int32 InLevel);

	int32 GetLevel() const { return Level; }

	/**
	 * 보스인가. DT_MonsterDefinition 에서 읽어 둔 값이다.
	 *
	 * 공격자의 Stat.Offense.BossDamage 가 이 대상에게만 적용된다
	 * (UTDCombatStatics::ApplyDamage). UI 가 보스 체력바를 크게 그릴 때도 쓸 수 있다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Monster")
	bool IsBoss() const { return bIsBoss; }
	
	/** 처치 보상 지급. 죽인 쪽의 성장 컴포넌트에 경험치를 넣는다. 서버 전용. */
	void GrantRewards(ATDCharacterBase* Killer);
	
	UPROPERTY(BlueprintAssignable, Category = "TD|Monster")
	FTDOnMonsterSensed OnSensed;

	UFUNCTION(BlueprintPure, Category = "TD|Monster")
	FName GetMonsterId() const
	{
		return MonsterId;
	}

	// ── 인스턴스(그룹 전용 몬스터) ─────────────────────────
	//
	// 사냥터 몬스터는 "이 몬스터는 누구 것인가" 를 들고 다닌다. 값이 같은 사람에게만
	// 복제되고(IsNetRelevantFor), 그 사람만 때릴 수 있다(FilterByTeam).
	// 키가 비어 있으면 **모두의 것**이다 — 보스방·배치형 몬스터가 그렇다.
	//
	// 키는 ATDPlayerState::GetInstanceGroupId() 에서 온다(파티면 PartyId, 아니면 개인).

	/** 비어 있으면 누구나 보고 때릴 수 있다. */
	FGuid GetOwnerGroupId() const { return OwnerGroupId; }

	/** 서버 전용. 스폰 포인트가 몬스터를 만들자마자 넣는다. */
	void SetOwnerGroupId(const FGuid& InGroupId) { OwnerGroupId = InGroupId; }

	/**
	 * 이 몬스터를 볼 수 있는 사람인가. 그룹 키가 없으면 누구나 볼 수 있다.
	 *
	 * 판정·AI·복제가 같은 답을 써야 "안 보이는데 맞는" 몬스터가 생기지 않으므로
	 * 세 곳이 모두 이 함수를 거친다.
	 */
	bool IsVisibleToGroup(const AActor* Other) const;

	/** 남의 몬스터는 아예 복제하지 않는다 — 그래서 화면에 나타나지 않는다. */
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget,
		const FVector& SrcLocation) const override;

	/**
	 * 이 플레이어와 서로 통과할지 정한다. **서버 전용.**
	 *
	 * 콜리전 설정을 끄는 것이 아니라 "이 둘은 이동할 때 서로 무시" 쌍을 지정한다.
	 * 그래서 내 몬스터와 보스는 그대로 막고, 보이지도 않는 남의 몬스터만 통과한다.
	 *
	 * 이동하는 쪽이 자기 무시 목록을 보므로 **양쪽에 모두 걸어야** 한다 —
	 * 한쪽만 걸면 그쪽만 통과하고 반대 방향에서는 여전히 막힌다.
	 */
	void SetMoveIgnoredByPawn(APawn* Pawn, bool bIgnore);
	/** "발견!" 방송. AI 컨트롤러가 부른다. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnSense();
	
	//  MonsterId 복제
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
protected:
	virtual void BeginPlay() override;

	/** 테이블 행을 읽어 스탯 컴포넌트의 기본값을 채운다. 서버 전용. */
	void ApplyDefinition();
	
	/** 사망 시 콜리전·이동을 멈추고 시체를 잠시 남긴 뒤 파괴한다. */
	virtual void HandleDeath() override;

	/** 사망 후 시체가 남아 있는 시간(초). 이후 액터가 파괴된다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Monster", meta = (ClampMin = "0"))
	float CorpseLifetime = 3.f;

	/** DT_MonsterDefinition. RowName 이 MonsterId 다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Monster")
	TObjectPtr<UDataTable> MonsterTable;

	/** 조회할 행 이름. */
	UPROPERTY(EditAnywhere, Replicated, Category = "TD|Monster")
	FName MonsterId;

	/**
	 * 이 몬스터의 주인 그룹. 복제하지 않는다 — 묻는 곳이 전부 서버다.
	 *
	 * 비어 있으면 공용 몬스터다. 레벨에 직접 배치한 몬스터와 보스가 그렇다.
	 */
	FGuid OwnerGroupId;

	/** FScalableFloat 커브를 읽을 레벨. */
	UPROPERTY(EditAnywhere, Category = "TD|Monster", meta = (ClampMin = "1"))
	int32 Level = 1;
	
	/** 체력바 위젯을 찾아 어트리뷰트 변경 델리게이트에 연결한다. 렌더링하는 머신에서만. */
	void SetupHealthBar();

	/**
	 * 피격음. OnDamaged 가 부른다 — 그 방송은 서버가 ReceiveHit 에서 쏘고 전 머신이 받으므로,
	 * 각자 자기 화면에서 재생하면 된다. 체력 변화를 보고 짐작하지 않는 이유는 회복·도트 때문이다.
	 *
	 * 소리는 DT_MonsterDefinition 의 HitSFX 에서 읽는다. 지정하지 않은 몬스터는 조용하다.
	 */
	UFUNCTION()
	void HandleDamagedForSound(AActor* Attacker, float Damage, bool bCritical);

private:
	/** 스탯 계산 결과를 어트리뷰트에 기록한다. 플레이어의 PlayerState 가 하는 일과 같다. */
	void UpdateVitalAttributes();

	/**
	 * 드롭 목록을 굴려 나온 아이템을 인벤토리에 넣는다. 서버 전용.
	 *
	 * 받는 사람은 파티장이다. 파티가 없거나 파티장이 다른 존에 있으면 처치자 본인이 받는다.
	 */
	void GrantDrops(ATDPlayerState* KillerPlayerState);

	/** ApplyDefinition 때 테이블에서 읽어둔 처치 경험치. */
	int32 ExpReward = 0;

	/**
	 * 처치 골드의 범위. 경험치와 같은 자리에서 레벨 스케일로 읽어둔다.
	 *
	 * FScalableFloat 라 커브를 붙이면 레벨이 높은 사냥터일수록 자동으로 많아지고,
	 * 안 붙이면 상수로 동작한다.
	 */
	int32 GoldMinReward = 0;
	int32 GoldMaxReward = 0;

	/** ApplyDefinition 때 읽어둔 드롭 목록 이름. 비어 있으면 아이템을 떨구지 않는다. */
	FName DropTableId;

	/**
	 * ApplyDefinition 때 테이블에서 읽어둔 보스 여부.
	 *
	 * 복제하지 않는다 — 피해 계산은 서버만 하고, 클라이언트가 알아야 할 일이 생기면
	 * 그때 UI 용으로 복제를 붙이면 된다.
	 */
	bool bIsBoss = false;

	/** 마지막 프레임에 여럿이 동시 타격해도 보상은 한 번만 나가게 하는 표시. */
	bool bRewardsGranted = false;
	
	UPROPERTY(VisibleAnywhere, Category = "TD|Stats")
	TObjectPtr<UTDStatComponent> StatComponent;

	UPROPERTY(VisibleAnywhere, Category = "TD|Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UTDAttributeSet> AttributeSet;
	
	void HandleVitalChangedForUI(const FOnAttributeChangeData& Data);

	UPROPERTY()
	TObjectPtr<UTDEnemyHealthBarWidget> HealthBarWidget;
	

};
