#pragma once

#include "CoreMinimal.h"
#include "Combat/TDCombatCalculation.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TDCombatStatics.generated.h"

class ATDCharacterBase;

/**
 * 데미지 적용의 단일 진입점. 평타·스킬·몬스터 공격·보스 패턴·함정이 전부 여기를 거친다.
 * 서버 전용 — 클라이언트에서 부르면 아무 일도 하지 않는다.
 */
UCLASS()
class TD_PROJECT_API UTDCombatStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 공격자 스탯으로 데미지를 계산해 대상에게 적용한다.
	 *
	 * @param ContextTags        시전 상황 태그. 조건부 장비 옵션("화염 스킬 +30%")이 여기서 평가된다.
	 * @param DamageMultiplier   공격력 배율. 스킬은 DT_SkillEffect 값을, 보스는 패턴 스펙의 DamageScale 을 넘긴다.
	 *                           1.5 면 공격력의 150%. 평타는 기본값 1 을 쓰므로 호출부가 그대로다.
	 *                           크리와 방어 계산 **앞**에 곱해지므로 배율이 커도 방어가 정상적으로 깎는다.
	 *                           0 이하면 "피해 없음" — 바닥값(최소 1)에 걸려 때린 것이 되지 않게 한다.
	 */
	static FTDDamageResult ApplyDamage(AActor* Attacker, AActor* Target,
		const FGameplayTagContainer& ContextTags, float DamageMultiplier = 1.f);

	// ── 대상 수집 ─────────────────────────────────────────
	//
	// 평타·스킬·보스 패턴이 같은 규칙으로 대상을 고른다. 같은 팀 제외·시체 제외·중복 제거를
	// 여러 곳에 적으면 한쪽만 고쳐졌을 때 "스킬로는 아군이 맞는" 같은 일이 생긴다.
	// 아래 두 개는 "공격자 기준 정면/자기중심" 편의판(스킬·평타), 그 아래 두 개는
	// 중심·회전을 직접 주는 저수준판(보스 패턴)이다. 필터는 한 곳을 공유한다.

	/**
	 * 캐릭터 앞쪽 상자 안의 적.
	 *
	 * @param Direction      상자가 뻗는 방향. **액터 회전이 아니라 부르는 쪽이 정한다** —
	 *                       스프라이트는 마지막 이동 방향을 따라가므로(UTDCombatComponent::
	 *                       GetFacingDirection) 액터 회전으로 판정하면 화면과 어긋난다.
	 *                       0 벡터면 액터 정면으로 떨어진다
	 * @param HalfExtent     상자 절반 크기. 전체 크기가 아니다
	 * @param ForwardOffset  몸 중심에서 상자 중심까지의 거리
	 */
	static TArray<AActor*> GatherTargetsInBox(const AActor* Attacker, FVector Direction,
		FVector HalfExtent, float ForwardOffset, bool bDrawDebug = false);

	/** 캐릭터를 중심으로 한 구 안의 적. 앞뒤를 가리지 않는다. */
	static TArray<AActor*> GatherTargetsInSphere(const AActor* Attacker,
		float Radius, bool bDrawDebug = false);

	/** 중심·회전을 직접 주는 박스 판정. 보스 패턴처럼 "찍어둔 그 자리"를 때릴 때. */
	static TArray<ATDCharacterBase*> GatherEnemiesInBox(const ATDCharacterBase* Attacker,
		const FVector& Center, const FQuat& Rotation, const FVector& Extent, bool bDrawDebug = false);

	/** 중심을 직접 주는 구 판정. */
	static TArray<ATDCharacterBase*> GatherEnemiesInSphere(const ATDCharacterBase* Attacker,
		const FVector& Center, float Radius, bool bDrawDebug = false);

	/**
	 * 계산 없이 정해진 수치를 그대로 적용한다. 디버그 명령과 낙하 피해 같은
	 * "공격자 없는 피해"용. ApplyDamage 의 마지막 구간과 같은 경로를 쓴다.
	 */
	static void ApplyRawDamage(AActor* Target, float Amount);

	/**
	 * 체력 회복의 단일 진입점. 포션·회복 스킬·리젠이 전부 여기를 거친다.
	 *
	 * 데미지와 달리 지금은 GE 를 태우지 않고 어트리뷰트를 직접 올린다.
	 * 회복에는 방어력 계산도 사망 판정도 없어서 우편함(메타 어트리뷰트)이 필요 없기 때문이다.
	 * "회복량 +20%" 나 "회복 불가" 같은 것이 생기면 이 함수 안쪽만 GE 경로로 바꾸면 되고,
	 * 부르는 쪽은 손대지 않아도 된다 — 진입점을 하나로 두는 이유가 그것이다.
	 *
	 * @return 실제로 회복이 일어났으면 true. 이미 가득 찼거나 죽었으면 false.
	 */
	static bool RestoreHealth(AActor* Target, float Amount);

	/** 마나 회복. RestoreHealth 와 같은 규칙을 따른다. */
	static bool RestoreMana(AActor* Target, float Amount);

	/**
	 * 마나 소모. RestoreMana 의 짝이다.
	 *
	 * 모자라면 **아무것도 하지 않고** false 를 돌려준다. 부분 소모가 없는 이유는
	 * 부르는 쪽이 전부 "쓸 수 있으면 쓴다" 이기 때문이다 — 마나가 절반만 있다고
	 * 스킬이 절반만 나가지는 않는다.
	 *
	 * Amount 가 0 이하면 소모할 것이 없으므로 true 다(공짜 스킬).
	 */
	static bool ConsumeMana(AActor* Target, float Amount);

	/** 현재 마나. 어트리뷰트가 없으면 0. 쓰기 전에 미리 걸러내는 용도다. */
	static float GetMana(const AActor* Actor);

	/**
	 * 이 캐릭터가 안전지대에 있는가. `DT_ZoneEnvironment.bIsSafeZone` 을 읽는다.
	 *
	 * 몬스터는 늘 false 다 — 존은 플레이어에게만 있는 값이라(PlayerState) 몬스터의
	 * 위치로는 판단할 수 없다. 안전지대에 몬스터를 두지 않는 것이 전제다.
	 *
	 * UI 가 "안전지대" 표시를 하려면 이 함수를 쓰면 된다. 다만 **서버 전용**이다 —
	 * GameMode 에서 테이블을 읽으므로 클라이언트에서는 항상 false 가 나온다.
	 * 클라이언트에서 판단해야 하면 CurrentZoneId 로 테이블을 직접 조회할 것.
	 */
	static bool IsInSafeZone(const AActor* Actor);
};
