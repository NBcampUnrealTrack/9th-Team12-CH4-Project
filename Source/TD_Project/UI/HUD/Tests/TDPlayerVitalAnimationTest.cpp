#include "UI/HUD/TDProgressBarAnimation.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTDPlayerVitalAnimationTest,
                                 "TD.UI.PlayerStatus.VitalAnimation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::
                                 EngineFilter)

bool FTDPlayerVitalAnimationTest::RunTest(const FString& Parameters)
{
	auto Update = [](FTDProgressBarAnimation& State, float Current, float Maximum, bool bHP,
	                 bool bAnimate = true)
	{
		FTDProgressBarStyle Style;
		Style.Mode = bHP
			             ? ETDProgressBarAnimationMode::DamageTrail
			             : ETDProgressBarAnimationMode::Smooth;
		State.SetValue(Current, Maximum, Maximum > 0.f ? Current / Maximum : 0.f, bAnimate, Style);
	};
	FTDProgressBarAnimation HP;
	Update(HP, 875.f, 975.f, true);
	TestEqual(TEXT("Initial value is immediate"), HP.DisplayPercent, 875.f / 975.f);
	TestFalse(TEXT("No login animation"), HP.IsActive());
	Update(HP, 575.f, 975.f, true);
	TestEqual(TEXT("Damage foreground is immediate"), HP.DisplayPercent, 575.f / 975.f);
	TestEqual(TEXT("Old health remains in trail"), HP.TrailPercent, 875.f / 975.f);
	HP.Advance(0.1f);
	TestEqual(TEXT("Trail waits before shrinking"), HP.TrailPercent, 875.f / 975.f);
	HP.Advance(0.28f);
	TestTrue(
			TEXT("Trail moves after delay including frame overshoot"),
			HP.TrailPercent > HP.DisplayPercent && HP.TrailPercent < 875.f / 975.f);
	const float BeforeHit = HP.TrailPercent;
	Update(HP, 275.f, 975.f, true);
	TestEqual(TEXT("Repeated hit retargets from visible trail"), HP.TrailPercent, BeforeHit);
	HP.Advance(1.f);
	TestEqual(TEXT("Trail finishes exactly"), HP.TrailPercent, HP.DisplayPercent);
	TestFalse(TEXT("No pending work once complete"), HP.IsActive());

	Update(HP, 675.f, 975.f, true);
	TestEqual(TEXT("Recovery starts from displayed value"), HP.DisplayPercent, 275.f / 975.f);
	TestTrue(TEXT("Recovery flashes"), HP.FlashAlpha > 0.f);
	HP.Advance(0.125f);
	TestTrue(
			TEXT("Recovery smoothly fills"),
			HP.DisplayPercent > 275.f / 975.f && HP.DisplayPercent < 675.f / 975.f);
	TestEqual(TEXT("No damage trail on recovery"), HP.TrailPercent, HP.DisplayPercent);
	HP.Advance(1.f);
	TestEqual(TEXT("Recovery reaches target"), HP.DisplayPercent, 675.f / 975.f);
	TestEqual(TEXT("Recovery flash ends"), HP.FlashAlpha, 0.f);

	Update(HP, 675.f, 1075.f, true);
	TestFalse(TEXT("Maximum change is not damage"), HP.IsActive());
	TestEqual(TEXT("Maximum change snaps ratio"), HP.DisplayPercent, 675.f / 1075.f);
	Update(HP, 900.f, 1075.f, true, false);
	TestFalse(TEXT("Effects can be disabled"), HP.IsActive());
	TestEqual(TEXT("Disabled effects snap"), HP.DisplayPercent, 900.f / 1075.f);
	Update(HP, 0.f, 1075.f, true);
	HP.Advance(1.f);
	TestEqual(TEXT("Death ends at zero including trail"), HP.TrailPercent, 0.f);

	FTDProgressBarAnimation MP;
	Update(MP, 250.f, 320.f, false);
	Update(MP, 150.f, 320.f, false);
	TestEqual(TEXT("Mana starts at old display"), MP.DisplayPercent, 250.f / 320.f);
	MP.Advance(0.1f);
	TestTrue(
			TEXT("Mana is interpolated"),
			MP.DisplayPercent > 150.f / 320.f && MP.DisplayPercent < 250.f / 320.f);
	const float BeforeCost = MP.DisplayPercent;
	Update(MP, 50.f, 320.f, false);
	TestEqual(TEXT("Repeated mana cost does not jump backwards"), MP.DisplayPercent, BeforeCost);
	MP.Advance(0.2f);
	TestEqual(TEXT("Mana finishes in 0.2 seconds"), MP.DisplayPercent, 50.f / 320.f);
	TestFalse(TEXT("Spending mana does not flash"), MP.IsActive());
	MP.Reset();
	Update(MP, 300.f, 320.f, false);
	TestFalse(TEXT("Rebinding has no stale animation"), MP.IsActive());
	Update(MP, 0.f, 0.f, false);
	TestEqual(TEXT("Missing maximum is safe"), MP.DisplayPercent, 0.f);
	return true;
}
#endif
