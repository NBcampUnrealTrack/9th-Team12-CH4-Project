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
};