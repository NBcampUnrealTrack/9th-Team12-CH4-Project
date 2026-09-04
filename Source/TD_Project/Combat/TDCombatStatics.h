#pragma once

#include "CoreMinimal.h"
#include "Combat/TDCombatCalculation.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TDCombatStatics.generated.h"

/**
 * 데미지 적용의 단일 진입점. 평타·스킬·몬스터 공격·함정이 전부 여기를 거친다.
 * 서버 전용 — 클라이언트에서 부르면 아무 일도 하지 않는다.
 */
UCLASS()
class TD_PROJECT_API UTDCombatStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 공격자 스탯으로 데미지를 계산해 대상에게 적용한다.
	 * @param ContextTags  시전 상황 태그. 조건부 장비 옵션("화염 스킬 +30%")이 여기서 평가된다.
	 */
	static FTDDamageResult ApplyDamage(AActor* Attacker, AActor* Target,
		const FGameplayTagContainer& ContextTags);

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