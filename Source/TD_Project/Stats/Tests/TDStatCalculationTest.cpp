#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "Stats/TDStatCalculation.h"

#if WITH_DEV_AUTOMATION_TESTS

// 테스트 전용 태그. S1에서 정의할 실제 Stat.* 계층과 겹치지 않도록 Test. 네임스페이스를 쓴다.
// 계산 규칙 검증에는 계층 관계만 있으면 되고 태그 이름 자체는 상관없다.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Damage, "Test.Damage");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Damage_Fire, "Test.Damage.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Damage_Physical, "Test.Damage.Physical");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Context_FireSkill, "Test.Context.FireSkill");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTDStatCalculationTest,
	"TD.Stats.Calculation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDStatCalculationTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Damage = TAG_Test_Damage.GetTag();
	const FGameplayTag Fire = TAG_Test_Damage_Fire.GetTag();
	const FGameplayTag Physical = TAG_Test_Damage_Physical.GetTag();
	const FGameplayTag FireSkill = TAG_Test_Context_FireSkill.GetTag();

	// ── 기본 ────────────────────────────────────────────────
	{
		const TArray<FTDStatModifier> Mods;
		TestEqual(TEXT("모디파이어가 없으면 기본값 그대로"),
			TDStat::Calculate(Damage, 100.f, Mods), 100.f);
	}

	// ── Base / Added 는 함께 합산된다 ────────────────────────
	{
		const TArray<FTDStatModifier> Mods = {
			{ Damage, ETDModOp::Added, 30.f },
			{ Damage, ETDModOp::Added, 20.f }
		};
		TestEqual(TEXT("Added 는 서로 더해진다"),
			TDStat::Calculate(Damage, 100.f, Mods), 150.f);
	}
	{
		// Base와 Added를 타입으로 나눠둔 것은 의미론적 구분이지 계산 차이가 아니다.
		const TArray<FTDStatModifier> Mods = {
			{ Damage, ETDModOp::Base,  40.f },
			{ Damage, ETDModOp::Added, 10.f }
		};
		TestEqual(TEXT("Base 와 Added 는 같은 층에서 합산된다"),
			TDStat::Calculate(Damage, 100.f, Mods), 150.f);
	}

	// ── Increased 는 합연산 ──────────────────────────────────
	{
		const TArray<FTDStatModifier> Mods = {
			{ Damage, ETDModOp::Increased, 0.10f },
			{ Damage, ETDModOp::Increased, 0.15f }
		};
		// 100 × (1 + 0.10 + 0.15)
		TestEqual(TEXT("Increased 는 더해진 뒤 한 번만 곱해진다"),
			TDStat::Calculate(Damage, 100.f, Mods), 125.f);
	}

	// ── More 는 곱연산 ───────────────────────────────────────
	{
		const TArray<FTDStatModifier> Mods = {
			{ Damage, ETDModOp::More, 0.10f },
			{ Damage, ETDModOp::More, 0.15f }
		};
		// 100 × 1.10 × 1.15 — 같은 수치라도 Increased(125)보다 크다
		TestEqual(TEXT("More 는 각각 따로 곱해진다"),
			TDStat::Calculate(Damage, 100.f, Mods), 126.5f, 0.001f);
	}

	// ── 네 연산이 섞인 경우 ──────────────────────────────────
	{
		const TArray<FTDStatModifier> Mods = {
			{ Damage, ETDModOp::Base,      30.f },
			{ Damage, ETDModOp::Added,     20.f },
			{ Damage, ETDModOp::Increased, 0.20f },
			{ Damage, ETDModOp::Increased, 0.30f },
			{ Damage, ETDModOp::More,      0.50f }
		};
		// (100 + 30 + 20) × (1 + 0.20 + 0.30) × 1.50 = 150 × 1.5 × 1.5
		TestEqual(TEXT("(Base+Added) × (1+ΣIncreased) × Π(1+More)"),
			TDStat::Calculate(Damage, 100.f, Mods), 337.5f, 0.001f);
	}

	// ── 태그 계층 매칭 ───────────────────────────────────────
	{
		const TArray<FTDStatModifier> Mods = {
			{ Damage, ETDModOp::Increased, 0.10f },	// "모든 피해 +10%"
			{ Fire,   ETDModOp::Increased, 0.15f }	// "화염 피해 +15%"
		};

		// 화염 피해에는 상위 Damage 모디파이어까지 걸린다 → 25%
		TestEqual(TEXT("하위 스탯 계산에 상위 태그 모디파이어가 포함된다"),
			TDStat::Calculate(Fire, 100.f, Mods), 125.f);

		// 물리 피해에는 Damage 만 걸린다. 형제 태그인 Fire 는 무관하다.
		TestEqual(TEXT("형제 태그 모디파이어는 걸리지 않는다"),
			TDStat::Calculate(Physical, 100.f, Mods), 110.f);

		// 반대 방향은 성립하지 않는다 — 상위 스탯에 하위 모디파이어가 새면 안 된다.
		TestEqual(TEXT("상위 스탯 계산에 하위 태그 모디파이어는 포함되지 않는다"),
			TDStat::Calculate(Damage, 100.f, Mods), 110.f);
	}

	// ── 조건부 모디파이어 ────────────────────────────────────
	{
		FTDStatModifier Conditional(Damage, ETDModOp::Increased, 0.30f);
		Conditional.Condition = FGameplayTagQuery::MakeQuery_MatchTag(FireSkill);

		const TArray<FTDStatModifier> Mods = { Conditional };

		// 컨텍스트 없이 계산하는 경우(캐릭터 시트에 캐시할 값)는 조건부가 빠진다.
		TestEqual(TEXT("컨텍스트가 없으면 조건부는 제외된다"),
			TDStat::Calculate(Damage, 100.f, Mods), 100.f);

		// 조건을 만족하는 컨텍스트(화염 스킬 시전)에서는 포함된다.
		const FGameplayTagContainer FireContext(FireSkill);
		TestEqual(TEXT("조건을 만족하면 포함된다"),
			TDStat::Calculate(Damage, 100.f, Mods, FireContext), 130.f);

		// 조건과 무관한 컨텍스트에서는 여전히 빠진다.
		const FGameplayTagContainer OtherContext(Physical);
		TestEqual(TEXT("조건이 맞지 않는 컨텍스트에서는 제외된다"),
			TDStat::Calculate(Damage, 100.f, Mods, OtherContext), 100.f);
	}

	// ── 클램프 ───────────────────────────────────────────────
	{
		// 저항 상한(75%)처럼 넘으면 안 되는 스탯을 위한 것.
		const TArray<FTDStatModifier> Mods = { { Damage, ETDModOp::Added, 100.f } };
		TestEqual(TEXT("상한을 넘으면 잘린다"),
			TDStat::Calculate(Damage, 100.f, Mods, FGameplayTagContainer::EmptyContainer, 0.f, 150.f), 150.f);
	}
	{
		const TArray<FTDStatModifier> Mods = { { Damage, ETDModOp::Added, -150.f } };
		TestEqual(TEXT("하한 아래로 내려가면 잘린다"),
			TDStat::Calculate(Damage, 100.f, Mods, FGameplayTagContainer::EmptyContainer, 0.f, 150.f), 0.f);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
