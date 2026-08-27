#include "Misc/AutomationTest.h"
#include "Combat/TDCombatCalculation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTDCombatCalculationTest,
	"TD.Combat.Calculation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDCombatCalculationTest::RunTest(const FString& Parameters)
{
	// ── 기본: 방어 0, 크리 없음 ──
	{
		FTDDamageInput Input;
		Input.AttackDamage = 100.f;

		// CritRoll 0.99 → CritChance 0 이므로 크리 아님
		const FTDDamageResult Result = TDCombat::CalculateDamage(Input, 0.99f);

		TestEqual(TEXT("방어 0 이면 공격력 그대로"), Result.FinalDamage, 100.f);
		TestFalse(TEXT("크리 확률 0 이면 크리 없음"), Result.bCritical);
	}

	// TODO: 아래 케이스들을 직접 작성
	// - 크리 강제: CritChance 1.0, CritRoll 0.0 → 100 × 1.5 = 150, bCritical true
	// - 방어 적용: Armor 100 → 100 × 100/200 = 50
	// - 방어무시 초과: Armor 50, Pen 80 → 유효방어 0 → 100
	// - 피해감소: DR 0.5 → 절반
	// - 전부 조합: 수기로 기대값 계산해서 명세로 남기기
	// - 바닥값: AttackDamage 0 → 최소 1

	return true;
}

#endif