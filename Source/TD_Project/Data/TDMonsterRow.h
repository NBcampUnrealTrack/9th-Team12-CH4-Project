#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ScalableFloat.h"
#include "TDMonsterRow.generated.h"

/**
 * DT_MonsterDefinition 의 행. 몬스터 1종 = 1행이다.
 *
 * 레벨을 행으로 만들지 않는다. MonsterId × Level 로 행을 뽑으면 20종 × 50레벨 = 1000행이 되고,
 * "고블린 전체를 약하게"가 50행 수정 작업이 된다.
 *
 * 대신 스탯을 FScalableFloat 로 둔다. 커브를 붙이지 않으면 그냥 상수로 동작하고,
 * 특정 레벨 구간만 조절해야 할 때 CurveTable 을 연결하면 키 하나로 잡을 수 있다.
 * 이때 주변 레벨은 보간으로 따라오므로 49/51 레벨에 역전이 생기지 않는다.
 *
 * 식별자는 RowName 이다. MonsterId 필드를 따로 두면 같은 값이 두 곳에 존재하게 된다.
 */
USTRUCT(BlueprintType)
struct FTDMonsterRow : public FTableRowBase
{
	GENERATED_BODY()

	/** UI·로그 표시용 이름. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster")
	FText DisplayName;

	/**
	 * 보스인가. `Stat.Offense.BossDamage` 가 이 몬스터에게만 붙는다.
	 *
	 * ArchetypeTag 로 표현하지 않은 이유는 축이 다르기 때문이다 — 아키타입은
	 * "어떻게 싸우는가"(Brute/Caster)이고, 보스는 그와 무관하게 정해진다.
	 * 힘형 보스도 마법형 보스도 있을 수 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster")
	bool bIsBoss = false;

	/**
	 * 분류 태그. 예: Monster.Type.Brute
	 *
	 * 스탯을 공유하는 필드가 아니다. 같은 아키타입이어도 스탯은 행마다 개별이며,
	 * 이 태그는 AI 선택, 저항 패턴, "언데드에게 추가 피해" 같은 조건부 모디파이어의
	 * 대상 판정에 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster")
	FGameplayTag ArchetypeTag;

	// ── 스탯 ──────────────────────────────────────────────
	// 전부 GetValueAtLevel(Level) 로 읽는다.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Stats")
	FScalableFloat BaseHealth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Stats")
	FScalableFloat BaseDamage;

	/**
	 * BaseDamage 가 어느 공격 타입으로 들어갈지. 비워두면 물리로 취급한다.
	 *
	 * 상위 태그인 Stat.Offense.Damage 에 넣으면 안 된다. 기본값은 계층을 타지 않아서
	 * 하위 타입 계산에 반영되지 않기 때문이다. 반드시 Physical 이나 Magical 을 지정한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Stats")
	FGameplayTag DamageType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Stats")
	FScalableFloat BaseDefense;

	/**
	 * 권장 전투력. 도감이나 레이드 입장 화면에서 "이 정도는 돼야 한다"를 보여주는 값이다.
	 *
	 * 스탯에서 계산하지 않고 기획이 직접 적는다. 기믹 난이도, 페이즈 구성, 전제하는 파티 인원은
	 * 스탯에 전혀 나타나지 않아서, 같은 수치의 보스라도 실제 요구 스펙이 크게 달라진다.
	 *
	 * 플레이어의 UTDStatComponent::GetCombatPower() 와 같은 척도로 적어야 비교가 의미를 가진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Stats")
	FScalableFloat RecommendedCombatPower;

	// ── 보상 ──────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Reward")
	FScalableFloat ExpReward;

	/**
	 * 골드 하한·상한. 둘 다 FScalableFloat 이므로 같은 커브를 참조하고 Value 만 다르게 두면
	 * 커브 하나로 범위 전체가 레벨을 따라 움직인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Reward")
	FScalableFloat GoldMin;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Reward")
	FScalableFloat GoldMax;

	/** DT_DropTable 참조. 중첩 배열을 피하려고 테이블을 분리하고 ID 로만 가리킨다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Reward")
	FName DropTableId;

	// ── 표현 ──────────────────────────────────────────────

	/**
	 * 스폰할 몬스터 클래스. 반드시 소프트 참조여야 한다.
	 *
	 * 하드 참조로 두면 테이블을 여는 것만으로 등록된 모든 몬스터의 메시·애니메이션·머티리얼이
	 * 한꺼번에 메모리에 올라온다.
	 *
	 * 타입을 APawn 으로 둔 것은 데이터가 특정 게임플레이 클래스에 묶이지 않게 하려는 것이다.
	 * 실제 스폰 시점에 ATDEnemyBase 로 캐스팅해서 확인한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Presentation")
	TSoftClassPtr<APawn> EnemyClass;
};
