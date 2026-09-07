#pragma once

#include "CoreMinimal.h"
#include "Character/TDCharacterBase.h"
#include "TDEnemyBase.generated.h"

/** 적 발견 반응. 발견 모션·경계음이 구독한다. 전 머신에서 불린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnMonsterSensed);

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
	
	/** 처치 보상 지급. 죽인 쪽의 성장 컴포넌트에 경험치를 넣는다. 서버 전용. */
	void GrantRewards(ATDCharacterBase* Killer);
	
	UPROPERTY(BlueprintAssignable, Category = "TD|Monster")
	FTDOnMonsterSensed OnSensed;

	UFUNCTION(BlueprintPure, Category = "TD|Monster")
	FName GetMonsterId() const
	{
		return MonsterId;
	}
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

	/** FScalableFloat 커브를 읽을 레벨. */
	UPROPERTY(EditAnywhere, Category = "TD|Monster", meta = (ClampMin = "1"))
	int32 Level = 1;
	
	/** 체력바 위젯을 찾아 어트리뷰트 변경 델리게이트에 연결한다. 렌더링하는 머신에서만. */
	void SetupHealthBar();

private:
	/** 스탯 계산 결과를 어트리뷰트에 기록한다. 플레이어의 PlayerState 가 하는 일과 같다. */
	void UpdateVitalAttributes();

	/** ApplyDefinition 때 테이블에서 읽어둔 처치 경험치. */
	int32 ExpReward = 0;

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
