#pragma once

#include "AIController.h"
#include "CoreMinimal.h"
#include "TDMonsterAIController.generated.h"

class ATDCharacterBase;

/**
 * 필드 몬스터의 두뇌. 반경 어그로 → 추적 → 사거리 내 공격.
 *
 * 감지는 시야각 없는 반경 방식이다(MMO 필드몹 표준). 시야각이 필요해지면
 * FindNearestEnemy 만 교체하면 된다.
 *
 * 공격 자체는 CombatComponent 에 위임한다 — 쿨타임·히트박스·데미지가
 * 플레이어와 완전히 같은 경로를 타므로, 밸런스를 한 곳에서 관리할 수 있다.
 *
 * 서버 전용으로 동작한다. AI 는 데디케이티드 서버에만 존재하는 로직이다.
 */
UCLASS()
class TD_PROJECT_API ATDMonsterAIController : public AAIController
{
	GENERATED_BODY()

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** 이 반경 안에 적이 들어오면 어그로가 잡힌다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|AI", meta = (ClampMin = "0"))
	float AggroRadius = 800.f;

	/** 어그로 대상이 이보다 멀어지면 포기한다. Aggro 보다 커야 경계에서 떨림이 없다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|AI", meta = (ClampMin = "0"))
	float LoseAggroRadius = 1400.f;

	/** 이 거리 안이면 이동을 멈추고 공격한다. 히트박스 길이와 맞춰야 헛방이 없다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|AI", meta = (ClampMin = "0"))
	float AttackRange = 150.f;

	/** 판단 주기(초). Tick 대신 타이머를 쓴다 — 몬스터 수십 마리가 매 프레임 생각할 이유가 없다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|AI", meta = (ClampMin = "0.05"))
	float ThinkInterval = 0.25f;

private:
	/** 주기적 판단. 상태 분기가 전부 여기 있다. */
	void Think();

	/** 어그로 반경 안의 가장 가까운 적. 없으면 nullptr. 감지 방식을 바꾸려면 여기만 고친다. */
	ATDCharacterBase* FindNearestEnemy() const;

	/** 빙의한 몬스터가 죽었을 때. OnDeath 델리게이트가 부른다. */
	UFUNCTION()
	void HandlePawnDeath();

	/** 지금 노리는 대상. 대상 액터가 파괴되면 자동으로 무효가 되는 약한 참조다. */
	TWeakObjectPtr<ATDCharacterBase> AggroTarget;

	FTimerHandle ThinkTimerHandle;
};