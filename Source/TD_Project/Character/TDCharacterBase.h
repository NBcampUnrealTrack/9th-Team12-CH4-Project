#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"
#include "TDCharacterBase.generated.h"

class UAbilitySystemComponent;
class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;
class UTDProgressionComponent;
class UTDStatComponent;
class UTDCombatComponent;

/** 캐릭터가 사망했을 때. AI 가 행동을 멈추거나 보상을 지급할 지점이다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDOnCharacterDeath);

/**
 * 플레이어와 몬스터의 공통 베이스.
 *
 * 스탯 컴포넌트를 직접 소유하지 않는다. 부착 위치가 자식마다 다르기 때문이다 —
 * 몬스터는 Pawn 에 붙지만, 플레이어는 사망 시 Pawn 이 파괴돼도 스탯·버프가 남아야 하므로
 * APlayerState 에 붙는다. 이 클래스는 경로만 제공한다.
 */
UCLASS(Abstract)
class TD_PROJECT_API ATDCharacterBase : public ACharacter, public IGenericTeamAgentInterface, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	
	ATDCharacterBase();

	UFUNCTION(BlueprintPure, Category = "TD|Combat")
	UTDCombatComponent* GetCombatComponent() const { return CombatComponent; }

	// ── 2D 표현 ───────────────────────────────────────────
	// 스프라이트를 그리는 것과 애니메이션을 고르는 것이 분리돼 있다.
	// ACharacter 가 갖고 있는 Mesh(SkeletalMesh)는 쓰지 않는다 — 지우면
	// ACharacter 의 기능이 깨지므로 그냥 비워둔다.

	UFUNCTION(BlueprintPure, Category = "TD|Visual")
	UPaperFlipbookComponent* GetSpriteComponent() const { return SpriteComponent; }

	UFUNCTION(BlueprintPure, Category = "TD|Visual")
	UPaperZDAnimationComponent* GetAnimationComponent() const { return AnimationComponent; }
	
	/**
	 * IAbilitySystemInterface.
	 *
	 * 스탯 컴포넌트와 마찬가지로 위치가 자식마다 다르다 —
	 * 플레이어는 PlayerState, 몬스터는 자기 자신. 기본 구현은 nullptr 을 돌려준다.
	 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ── IGenericTeamAgentInterface ────────────────────────
	// AIPerception 이 적·아군을 가르는 기준이다. 이게 없으면 AI 쪽에서 클래스를 일일이
	// 캐스팅해 판별해야 하고, 나중에 중립 몬스터나 소환수가 생기면 그 코드를 전부 고쳐야 한다.

	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override;

	// ── 사망 ──────────────────────────────────────────────

	/**
	 * 사망 알림. 지금은 아무도 브로드캐스트하지 않는다 —
	 * 실제 판정은 현재 체력을 AttributeSet 이 관리하는 S4 에서 붙는다.
	 *
	 * 구독 지점을 먼저 열어두는 이유는 협업 때문이다. AI 가 "죽으면 멈춘다"를
	 * 붙일 자리가 없으면, 나중에 사망 처리를 넣을 때 AI 쪽 코드를 고치게 된다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "TD|Combat")
	FTDOnCharacterDeath OnDeath;

	UFUNCTION(BlueprintPure, Category = "TD|Combat")
	bool IsDead() const { return bIsDead; }
	
	/**
	 * 사망 처리. 체력이 0 이 됐을 때 호출한다(S4 이후).
	 * 여러 번 불려도 한 번만 동작하므로 호출부에서 중복을 걱정하지 않아도 된다.
	 */
	virtual void HandleDeath();
	
	
	/**
	 * 이 캐릭터의 스탯 컴포넌트. 없으면 nullptr 가 발생한다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Stats")
	virtual UTDStatComponent* GetStatComponent() const;

	/**
	 * 이 캐릭터의 성장 컴포넌트. 없으면 nullptr 가 발생한다.
	 * 스탯 컴포넌트와 마찬가지로 부착 위치가 자식마다 다르다. 플레이어는 PlayerState 에 있고,
	 * 몬스터는 성장이라는 개념이 없어 애초에 갖지 않는다 — 몬스터에게 nullptr 은 정상이다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Progression")
	virtual UTDProgressionComponent* GetProgressionComponent() const;

	/**
	 * 스탯 조회 단축 경로. 컴포넌트가 아직 없으면 DefaultValue 를 돌려준다.
	 * 호출부마다 null 검사를 반복하지 않으려고 여기 모아둔다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Stats")
	float GetStat(FGameplayTag Stat, float DefaultValue = 0.f) const;

protected:
	virtual void BeginPlay() override;

	/**
	 * 스탯이 바뀌었을 때 캐릭터에 실제로 반영한다. 지금은 이동 속도만 다룬다.
	 *
	 * 스탯 컴포넌트의 OnStatsChanged 에 연결되며, Tick 으로 감시하지 않는다.
	 */
	UFUNCTION()
	virtual void HandleStatsChanged();

	/** 스탯 컴포넌트가 준비되면 변경 알림을 구독한다. 이미 구독했으면 아무 일도 하지 않는다. */
	void BindToStatComponent();

	/**
	 * 소속 팀. 값 배정(플레이어 0 / 몬스터 1 등)은 AI 담당과 맞춰야 하므로
	 * 코드에 박지 않고 블루프린트에서 지정한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Team")
	FGenericTeamId TeamId;

private:
	/** 중복 구독을 막기 위한 표시. 플레이어는 PlayerState 복제 시점이 일정하지 않아 여러 번 시도된다. */
	bool bBoundToStatComponent = false;

	/** 서버 기준 값이다. 복제는 사망 처리가 실제로 붙는 S4 에서 함께 정한다. */
	bool bIsDead = false;
	
	/** 플레이어와 몬스터가 같은 공격 경로를 쓴다. 쿨타임·히트박스는 아바타 소유라 Pawn 에 둔다. */
	UPROPERTY(VisibleAnywhere, Category = "TD|Combat")
	TObjectPtr<UTDCombatComponent> CombatComponent;

	/** 실제로 화면에 그려지는 스프라이트. PaperZD 가 이 컴포넌트의 플립북을 갈아 끼운다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TD|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperFlipbookComponent> SpriteComponent;

	/**
	 * 어떤 애니메이션을 재생할지 정하는 상태 머신.
	 *
	 * AnimInstanceClass 와 RenderComponentRef 는 플러그인에서 private 이라
	 * C++ 로 지정할 수 없다. **블루프린트에서 연결해야 한다** —
	 * 직업별 교체는 나중에 SetAnimInstanceClass() 로 한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TD|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperZDAnimationComponent> AnimationComponent;
};
