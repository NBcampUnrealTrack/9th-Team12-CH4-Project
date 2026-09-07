#include "UI/HUD/TDProgressBarAnimation.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTDEXPBarAnimationTest, "TD.UI.EXPBar.Animation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::
                                 EngineFilter)

bool FTDEXPBarAnimationTest::RunTest(const FString& Parameters)
{
	auto Update = [](FTDProgressBarAnimation& State, int32 Level, int32 Exp, float Percent,
	                 bool bAnimate = true)
	{
		FTDProgressBarStyle Style;
		Style.Mode = ETDProgressBarAnimationMode::LevelWrapped;
		Style.ChangeSeconds = 0.35f;
		Style.LevelFillSeconds = 0.3f;
		Style.LevelHoldSeconds = 0.1f;
		Style.FlashSeconds = 0.4f;
		State.SetWrappedTarget(Level, Exp, Percent, bAnimate, Style);
	};
	FTDProgressBarAnimation State;
	Update(State, 24, 1000, 0.6f);
	TestEqual(TEXT("Initial snapshot has no animation"), State.DisplayPercent, 0.6f);
	TestFalse(TEXT("Initial snapshot is idle"), State.IsActive());
	Update(State, 24, 1100, 0.8f);
	State.Advance(0.175f);
	TestTrue(
			TEXT("EXP gain smoothly fills"),
			State.DisplayPercent > 0.6f && State.DisplayPercent < 0.8f);
	const float Previous = State.DisplayPercent;
	Update(State, 24, 1150, 0.9f);
	TestEqual(
			TEXT("Repeated gains retarget from visible position"), State.DisplayPercent, Previous);
	State.Advance(1.f);
	TestEqual(TEXT("Gain ends exactly"), State.DisplayPercent, 0.9f);
	TestFalse(TEXT("Gain animation stops"), State.IsActive());

	Update(State, 25, 1300, 0.2f);
	State.Advance(0.3f);
	TestEqual(TEXT("Level up first fills to 100 percent"), State.DisplayPercent, 1.f);
	TestTrue(TEXT("Level boundary pulses"), State.FlashAlpha > 0.f);
	State.Advance(0.05f);
	TestEqual(TEXT("100 percent is briefly held"), State.DisplayPercent, 1.f);
	Update(State, 25, 1400, 0.4f);
	State.Advance(1.f);
	TestEqual(TEXT("Gain during wrap reaches latest target"), State.DisplayPercent, 0.4f);

	Update(State, 30, 5000, 0.5f);
	State.Advance(3.f);
	TestEqual(TEXT("Multiple level ups end at final progress"), State.DisplayPercent, 0.5f);
	TestFalse(TEXT("Large jumps do not leave a long queue"), State.IsActive());
	Update(State, 29, 4500, 0.8f);
	TestEqual(TEXT("Level correction snaps"), State.DisplayPercent, 0.8f);
	TestFalse(TEXT("Level correction does not pulse"), State.IsActive());
	Update(State, 29, 4600, 0.9f, false);
	TestEqual(TEXT("Animation can be disabled"), State.DisplayPercent, 0.9f);

	State.Reset();
	Update(State, 100, 90000, 1.f);
	TestEqual(TEXT("Max level remains full"), State.DisplayPercent, 1.f);
	TestFalse(TEXT("Rebinding is immediate"), State.IsActive());

	FTDProgressBarAnimation Instant;
	FTDProgressBarStyle InstantStyle;
	InstantStyle.Mode = ETDProgressBarAnimationMode::LevelWrapped;
	InstantStyle.ChangeSeconds = InstantStyle.LevelFillSeconds = 0.f;
	InstantStyle.LevelHoldSeconds = InstantStyle.FlashSeconds = 0.f;
	Instant.SetWrappedTarget(1, 0, 0.f, true, InstantStyle);
	Instant.SetWrappedTarget(4, 100, 0.25f, true, InstantStyle);
	Instant.Advance(0.1f);
	TestEqual(TEXT("Zero durations are safe"), Instant.DisplayPercent, 0.25f);
	TestFalse(TEXT("Zero duration queue terminates"), Instant.IsActive());
	return true;
}
#endif
