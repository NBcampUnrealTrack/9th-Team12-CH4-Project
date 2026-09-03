#pragma once

#include "CoreMinimal.h"

/** 게임 수치는 건드리지 않고 게이지의 표시 상태만 보관한다. */
struct FTDPlayerVitalAnimation
{
 float DisplayPercent = 0.f;
 float TrailPercent = 0.f;
 float FlashAlpha = 0.f;

 void Reset(float Current = 0.f, float Maximum = 0.f, float Percent = 0.f)
 {
  LastCurrent = Current;
  LastMaximum = Maximum;
  TargetPercent = FMath::Clamp(Percent, 0.f, 1.f);
  DisplayPercent = TrailPercent = TargetPercent;
  MoveDuration = TrailDuration = DelayRemaining = FlashRemaining = FlashAlpha = 0.f;
  bInitialized = Maximum > KINDA_SMALL_NUMBER;
 }

 void SetTarget(float Current, float Maximum, float Percent, bool bAnimate, bool bDamageTrail,
  float ChangeSeconds, float RecoverySeconds, float TrailDelaySeconds,
  float TrailSeconds, float FlashSeconds)
 {
  Percent = FMath::Clamp(Percent, 0.f, 1.f);
  // 첫 표시와 최대치 변경은 피격/회복이 아니다. 초기 로딩 때 번쩍이거나 잔상이 남지 않는다.
  if (!bInitialized || !bAnimate || !FMath::IsNearlyEqual(Maximum, LastMaximum)
   || Maximum <= KINDA_SMALL_NUMBER)
  {
   Reset(Current, Maximum, Percent);
   return;
  }
  if (FMath::IsNearlyEqual(Current, LastCurrent) && FMath::IsNearlyEqual(Percent, TargetPercent)) return;
  if (FMath::IsNearlyEqual(Current, LastCurrent))
  {
   Reset(Current, Maximum, Percent);
   return;
  }

  const bool bRecovered = Current > LastCurrent;
  LastCurrent = Current;
  LastMaximum = Maximum;
  TargetPercent = Percent;
  MoveFrom = DisplayPercent;
  MoveElapsed = TrailElapsed = 0.f;
  DelayRemaining = TrailDuration = FlashRemaining = FlashAlpha = 0.f;

  if (!bRecovered && bDamageTrail)
  {
   TrailFrom = FMath::Max3(TrailPercent, DisplayPercent, TargetPercent);
   TrailPercent = TrailFrom;
   DisplayPercent = TargetPercent;
   MoveDuration = 0.f;
   DelayRemaining = FMath::Max(0.f, TrailDelaySeconds);
   TrailDuration = FMath::Max(0.f, TrailSeconds);
   if (TrailDuration <= 0.f) TrailPercent = TargetPercent;
  }
  else
  {
   MoveDuration = FMath::Max(0.f, bRecovered ? RecoverySeconds : ChangeSeconds);
   if (MoveDuration <= 0.f) DisplayPercent = TargetPercent;
   TrailPercent = DisplayPercent;
   if (bRecovered)
   {
    FlashDuration = FMath::Max(0.f, FlashSeconds);
    FlashRemaining = FlashDuration;
    FlashAlpha = FlashDuration > 0.f ? 1.f : 0.f;
   }
  }
 }

 bool IsActive() const
 {
  return MoveDuration > 0.f || TrailDuration > 0.f || FlashRemaining > 0.f;
 }

 void Advance(float DeltaSeconds)
 {
  const float Delta = FMath::Max(0.f, DeltaSeconds);
  if (MoveDuration > 0.f)
  {
   MoveElapsed += Delta;
   DisplayPercent = Interpolate(MoveFrom, TargetPercent, MoveElapsed / MoveDuration);
   if (MoveElapsed >= MoveDuration)
   {
    DisplayPercent = TargetPercent;
    MoveDuration = 0.f;
   }
  }

  if (TrailDuration > 0.f)
  {
   // 한 프레임이 지연 시간보다 길어도 남은 시간만큼 정확히 진행한다.
   const float ConsumedDelay = FMath::Min(Delta, DelayRemaining);
   DelayRemaining -= ConsumedDelay;
   TrailElapsed += Delta - ConsumedDelay;
   TrailPercent = FMath::Max(DisplayPercent, Interpolate(TrailFrom, TargetPercent, TrailElapsed / TrailDuration));
   if (TrailElapsed >= TrailDuration)
   {
    TrailPercent = TargetPercent;
    TrailDuration = 0.f;
   }
  }
  else
  {
   TrailPercent = DisplayPercent;
  }

  if (FlashRemaining > 0.f)
  {
   FlashRemaining = FMath::Max(0.f, FlashRemaining - Delta);
   const float Alpha = FlashRemaining / FlashDuration;
   FlashAlpha = Alpha * Alpha;
  }
 }

private:
 static float Interpolate(float From, float To, float Progress)
 {
  const float Alpha = FMath::Clamp(Progress, 0.f, 1.f);
  return FMath::Lerp(From, To, Alpha * Alpha * (3.f - 2.f * Alpha));
 }

 bool bInitialized = false;
 float LastCurrent = 0.f;
 float LastMaximum = 0.f;
 float TargetPercent = 0.f;
 float MoveFrom = 0.f;
 float MoveElapsed = 0.f;
 float MoveDuration = 0.f;
 float TrailFrom = 0.f;
 float TrailElapsed = 0.f;
 float TrailDuration = 0.f;
 float DelayRemaining = 0.f;
 float FlashRemaining = 0.f;
 float FlashDuration = 0.f;
};
