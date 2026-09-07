#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "World/TDZoneEnvironmentData.h"
#include "TDZoneEnvironmentComponent.generated.h"

class ADirectionalLight;
class AExponentialHeightFog;
class ASkyLight;
class USpringArmComponent;

/**
 * 존이 바뀔 때 화면을 그 존에 맞게 바꾼다. **클라이언트 전용 표현이다.**
 *
 * PlayerController 에 붙인다. 카메라는 Pawn 에 있지만 Pawn 은 죽으면 사라지고,
 * PlayerState 는 카메라를 모른다. 컨트롤러가 둘 다 아는 유일한 자리다.
 *
 * ── 서버는 관여하지 않는다 ──
 * 서버가 정하는 것은 "지금 어느 존인가"(`CurrentZoneId`)까지다. 그 값이 복제되면
 * 각자의 화면에서 이 컴포넌트가 알아서 카메라와 라이팅을 맞춘다. 조명 값을 복제하면
 * 30명에게 같은 숫자를 30번 보내는 셈이 된다.
 *
 * 그래서 **로컬 컨트롤러에서만 동작한다.** 서버에는 접속자 수만큼 PlayerController 가
 * 있는데, 전부가 라이팅을 만지면 마지막에 처리된 사람의 존이 이긴다.
 *
 * ── 라이팅은 액터를 갈아끼우지 않는다 ──
 * SkyLight·DirectionalLight·ExponentialHeightFog 는 월드에 하나만 유효하다(D49).
 * 존마다 액터 세트를 두고 켜고 끄는 대신, 레벨에 배치된 하나의 값을 바꾼다.
 * 레벨에 그 액터가 없으면 그 항목만 조용히 넘어간다 — 아트가 아직 배치하지 않았어도
 * 카메라는 정상 동작해야 하기 때문이다.
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDZoneEnvironmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDZoneEnvironmentComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * 지금 존의 설정을 다시 적용한다.
	 *
	 * Pawn 이 새로 생겼을 때(접속·부활) 컨트롤러가 부른다. 카메라는 Pawn 에 딸려 있어
	 * 죽고 살아나면 기본값으로 돌아오기 때문이다.
	 */
	void ReapplyCurrentZone();

private:
	/** PlayerState 의 존 변경 알림을 받는다. */
	UFUNCTION()
	void HandleZoneChanged(FGameplayTag NewZoneId);

	/** 존 정의 → 표현 에셋 → 카메라·라이팅 값 결정. 에셋이 없으면 기본값을 쓴다. */
	void ApplyZone(FGameplayTag ZoneId);

	void ApplyLighting(const FTDZoneLightingSettings& Settings);

	/** PlayerState 구독을 건다. 접속 직후에는 PlayerState 가 아직 없을 수 있다. */
	bool TryBindPlayerState();

	USpringArmComponent* FindSpringArm() const;

	// ── 기본값 캡처 ───────────────────────────────────────
	// 존이 bOverride_ 를 켜지 않았을 때 되돌아갈 값이다.
	//
	// 프로젝트 세팅에 적어 두지 않고 **레벨과 BP 에서 읽는다.** 두 곳에 같은 값을
	// 적으면 한쪽을 고칠 때마다 다른 쪽도 맞춰야 하고, 어긋나면 존을 옮기는 순간
	// 화면이 튄다. 아트가 조명이나 카메라를 조정하면 여기가 자동으로 따라간다.

	/** 레벨에 배치된 조명 액터의 현재 값. BeginPlay 에서 한 번 읽는다. */
	void CaptureLightingDefaults();

	/**
	 * BP 의 스프링암·카메라에 설정된 값. Pawn 을 처음 잡았을 때 읽는다.
	 *
	 * **아무것도 적용하기 전에** 읽어야 한다. 존 값을 넣은 뒤에 읽으면 그 존의
	 * 카메라가 기본값이 되어버린다.
	 */
	void CaptureCameraDefaults(const USpringArmComponent* SpringArm);

	FTDZoneCameraSettings CapturedCamera;
	FTDZoneLightingSettings CapturedLighting;

	bool bCameraCaptured = false;
	bool bLightingCaptured = false;

	/**
	 * 카메라 목표값. 존이 바뀌면 여기가 먼저 바뀌고, Tick 이 현재값을 여기로 끌어간다.
	 *
	 * 곧바로 갈아끼우지 않는 이유는 텔레포트 직후 화면이 확 튀면 멀미가 나기 때문이다.
	 */
	FTDZoneCameraSettings TargetCamera;

	/** 남은 블렌드 시간(초). 0 이면 Tick 이 할 일이 없다. */
	float RemainingBlendTime = 0.f;

	/**
	 * PlayerState 를 아직 못 잡았을 때 참으로 둔다.
	 *
	 * 접속 순서상 컨트롤러가 PlayerState 보다 먼저 준비되는 경우가 있어,
	 * BeginPlay 한 번으로는 구독에 실패할 수 있다. 그때는 Tick 이 다시 시도한다.
	 */
	bool bWaitingForPlayerState = true;

	/**
	 * 라이팅 액터. 매번 월드를 훑지 않으려고 찾아 둔다.
	 *
	 * 약한 참조인 이유는 레벨 스트리밍으로 사라질 수 있어서다. 사라졌으면
	 * 그 항목만 건너뛴다.
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<ADirectionalLight> CachedSunLight;

	UPROPERTY(Transient)
	TWeakObjectPtr<ASkyLight> CachedSkyLight;

	UPROPERTY(Transient)
	TWeakObjectPtr<AExponentialHeightFog> CachedFog;

	bool bLightActorsCached = false;

	void CacheLightActors();
};
