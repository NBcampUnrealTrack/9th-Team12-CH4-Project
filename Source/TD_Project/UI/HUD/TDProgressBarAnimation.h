#pragma once

#include "CoreMinimal.h"
#include "UI/Common/TDProgressBarStyleDA.h"

/** HP, MP, EXP에서 공용으로 사용하는 표시 전용 프로그레스바 애니메이션. */
struct FTDProgressBarAnimation
{
	float DisplayPercent = 0.f;
	float TrailPercent = 0.f;
	float FlashAlpha = 0.f;

	void Reset()
	{
		DisplayPercent = TrailPercent = FlashAlpha = 0.f;
		Phase = EPhase::Idle;
		TrailDuration = TrailDelayRemaining = FlashRemaining = 0.f;
		bInitialized = false;
		bWrappedMode = false;
	}

	void SetValue(float Current, float Maximum, float Percent, bool bAnimate,
	              const FTDProgressBarStyle& Style)
	{
		Percent = FMath::Clamp(Percent, 0.f, 1.f);
		if (!bInitialized || bWrappedMode || !bAnimate || Maximum <= KINDA_SMALL_NUMBER
			|| !FMath::IsNearlyEqual(Maximum, LastMaximum)){
			SnapValue(Current, Maximum, Percent);
			return;
		}
		if (FMath::IsNearlyEqual(Current, LastCurrent) && FMath::IsNearlyEqual(
				Percent, TargetPercent)){
			return;
		}
		if (FMath::IsNearlyEqual(Current, LastCurrent)){
			SnapValue(Current, Maximum, Percent);
			return;
		}

		const bool bRecovered = Current > LastCurrent;
		LastCurrent = Current;
		LastMaximum = Maximum;
		TargetPercent = Percent;
		TrailDuration = TrailDelayRemaining = 0.f;

		if (!bRecovered && Style.Mode == ETDProgressBarAnimationMode::DamageTrail){
			TrailFrom = FMath::Max3(TrailPercent, DisplayPercent, TargetPercent);
			TrailPercent = TrailFrom;
			DisplayPercent = TargetPercent;
			Phase = EPhase::Idle;
			TrailElapsed = 0.f;
			TrailDelayRemaining = FMath::Max(0.f, Style.TrailDelaySeconds);
			TrailDuration = FMath::Max(0.f, Style.TrailSeconds);
			if (TrailDuration <= 0.f){
				TrailPercent = TargetPercent;
			}
		}
		else{
			BeginMove(TargetPercent, bRecovered ? Style.RecoverySeconds : Style.ChangeSeconds);
			TrailPercent = DisplayPercent;
			if (bRecovered){
				Pulse(1.f, Style.FlashSeconds);
			}
		}
	}

	void SetWrappedTarget(int32 Sequence, int64 Serial, float Percent, bool bAnimate,
	                      const FTDProgressBarStyle& Style)
	{
		Sequence = FMath::Max(0, Sequence);
		Percent = FMath::Clamp(Percent, 0.f, 1.f);
		if (!bInitialized || !bWrappedMode || !bAnimate || Sequence < TargetSequence || Serial <
			LastSerial
			|| (Sequence == TargetSequence && Percent + KINDA_SMALL_NUMBER < TargetPercent)){
			SnapWrapped(Sequence, Serial, Percent);
			return;
		}
		if (Sequence == TargetSequence && Serial == LastSerial && FMath::IsNearlyEqual(
				Percent, TargetPercent)){
			return;
		}

		const bool bAlreadyWrapping = Phase == EPhase::Hold
				|| (Phase == EPhase::Move && DisplaySequence < TargetSequence);
		TargetSequence = Sequence;
		TargetPercent = Percent;
		LastSerial = Serial;
		ActiveStyle = Style;
		Pulse(0.45f, Style.FlashSeconds);
		if (!bAlreadyWrapping){
			BeginWrappedMove();
		}
	}

	bool IsActive() const
	{
		return Phase != EPhase::Idle || TrailDuration > 0.f || FlashRemaining > 0.f;
	}

	void Advance(float DeltaSeconds)
	{
		const float Delta = FMath::Max(0.f, DeltaSeconds);
		AdvanceTrail(Delta);

		float Remaining = Delta;
		if (Phase == EPhase::Idle){
			AdvanceFlash(Remaining);
			return;
		}
		for (int32 Step = 0; Step < 16 && Phase != EPhase::Idle; ++Step){
			const float Used = FMath::Min(Remaining, FMath::Max(0.f, PhaseDuration - PhaseElapsed));
			PhaseElapsed += Used;
			Remaining -= Used;
			AdvanceFlash(Used);
			if (Phase == EPhase::Move){
				DisplayPercent = Interpolate(MoveFrom, MoveTarget,
				                             PhaseDuration > KINDA_SMALL_NUMBER
					                             ? PhaseElapsed / PhaseDuration
					                             : 1.f);
				if (!bWrappedMode){
					TrailPercent = DisplayPercent;
				}
			}
			if (PhaseElapsed + KINDA_SMALL_NUMBER < PhaseDuration){
				break;
			}
			CompletePhase();
			if (Remaining <= 0.f){
				break;
			}
		}
		if (Remaining > 0.f){
			AdvanceFlash(Remaining);
		}
	}

private:
	enum class EPhase : uint8 { Idle, Move, Hold };

	static float Interpolate(float From, float To, float Progress)
	{
		const float Alpha = FMath::Clamp(Progress, 0.f, 1.f);
		return FMath::Lerp(From, To, Alpha * Alpha * (3.f - 2.f * Alpha));
	}

	void SnapValue(float Current, float Maximum, float Percent)
	{
		LastCurrent = Current;
		LastMaximum = Maximum;
		TargetPercent = DisplayPercent = TrailPercent = Percent;
		Phase = EPhase::Idle;
		TrailDuration = TrailDelayRemaining = FlashRemaining = FlashAlpha = 0.f;
		bInitialized = Maximum > KINDA_SMALL_NUMBER;
		bWrappedMode = false;
	}

	void SnapWrapped(int32 Sequence, int64 Serial, float Percent)
	{
		DisplaySequence = TargetSequence = Sequence;
		LastSerial = Serial;
		TargetPercent = DisplayPercent = TrailPercent = Percent;
		Phase = EPhase::Idle;
		TrailDuration = TrailDelayRemaining = FlashRemaining = FlashAlpha = 0.f;
		bInitialized = true;
		bWrappedMode = true;
	}

	void BeginMove(float Target, float Seconds)
	{
		MoveFrom = DisplayPercent;
		MoveTarget = Target;
		PhaseElapsed = 0.f;
		PhaseDuration = FMath::Max(0.f, Seconds);
		Phase = EPhase::Move;
	}

	void BeginWrappedMove()
	{
		BeginMove(DisplaySequence < TargetSequence ? 1.f : TargetPercent,
		          DisplaySequence < TargetSequence
			          ? ActiveStyle.LevelFillSeconds
			          : ActiveStyle.ChangeSeconds);
	}

	void CompletePhase()
	{
		if (Phase == EPhase::Move){
			DisplayPercent = MoveTarget;
			if (bWrappedMode && DisplaySequence < TargetSequence){
				Phase = EPhase::Hold;
				PhaseElapsed = 0.f;
				PhaseDuration = FMath::Max(0.f, ActiveStyle.LevelHoldSeconds);
				Pulse(1.f, ActiveStyle.FlashSeconds);
				return;
			}
		}
		else if (Phase == EPhase::Hold){
			// 큰 레벨 점프는 마지막 세 단계만 보여 주어 긴 애니메이션 큐를 만들지 않는다.
			DisplaySequence = FMath::Max(DisplaySequence + 1, TargetSequence - 2);
			DisplayPercent = TrailPercent = 0.f;
			BeginWrappedMove();
			return;
		}
		Phase = EPhase::Idle;
		TrailPercent = DisplayPercent;
	}

	void AdvanceTrail(float Delta)
	{
		if (TrailDuration <= 0.f){
			return;
		}
		const float ConsumedDelay = FMath::Min(Delta, TrailDelayRemaining);
		TrailDelayRemaining -= ConsumedDelay;
		TrailElapsed += Delta - ConsumedDelay;
		TrailPercent = FMath::Max(DisplayPercent,
		                          Interpolate(TrailFrom, TargetPercent,
		                                      TrailElapsed / TrailDuration));
		if (TrailElapsed >= TrailDuration){
			TrailPercent = TargetPercent;
			TrailDuration = 0.f;
		}
	}

	void Pulse(float Strength, float Seconds)
	{
		FlashDuration = FMath::Max(0.f, Seconds);
		FlashPeak = FMath::Max(FlashAlpha, Strength);
		FlashRemaining = FlashDuration;
		FlashAlpha = FlashDuration > 0.f ? FlashPeak : 0.f;
	}

	void AdvanceFlash(float Delta)
	{
		if (FlashRemaining <= 0.f){
			return;
		}
		FlashRemaining = FMath::Max(0.f, FlashRemaining - Delta);
		const float Alpha = FlashDuration > KINDA_SMALL_NUMBER
			                    ? FlashRemaining / FlashDuration
			                    : 0.f;
		FlashAlpha = FlashPeak * Alpha * Alpha;
	}

	EPhase Phase = EPhase::Idle;
	bool bInitialized = false;
	bool bWrappedMode = false;
	float TargetPercent = 0.f;
	float MoveFrom = 0.f;
	float MoveTarget = 0.f;
	float PhaseElapsed = 0.f;
	float PhaseDuration = 0.f;
	float LastCurrent = 0.f;
	float LastMaximum = 0.f;
	float TrailFrom = 0.f;
	float TrailElapsed = 0.f;
	float TrailDuration = 0.f;
	float TrailDelayRemaining = 0.f;
	float FlashPeak = 0.f;
	float FlashRemaining = 0.f;
	float FlashDuration = 0.f;
	int32 DisplaySequence = 0;
	int32 TargetSequence = 0;
	int64 LastSerial = 0;
	FTDProgressBarStyle ActiveStyle;
};
