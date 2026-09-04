#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TDZoneEnvironmentData.generated.h"

class USoundBase;
class UTexture2D;

/**
 * 존별 카메라. 2.5D 라 던전은 좁게, 필드는 넓게 잡는 편이 자연스럽다.
 *
 * 값은 스프링암과 카메라에 그대로 들어간다.
 */
USTRUCT(BlueprintType)
struct FTDZoneCameraSettings
{
	GENERATED_BODY()

	/** 스프링암 길이. 클수록 멀리서 본다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "100.0"))
	float ArmLength = 800.f;

	/** 내려다보는 각도. 음수가 위에서 아래를 본다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "-89.0", ClampMax = "0.0"))
	float Pitch = -45.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float FieldOfView = 90.f;

	/**
	 * 존이 바뀔 때 새 값으로 옮겨가는 시간(초). 0 이면 즉시.
	 *
	 * 텔레포트 직후에 카메라가 확 움직이면 멀미가 나므로 짧게라도 두는 편이 낫다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0"))
	float BlendTime = 0.5f;
};

/**
 * 존별 라이팅. 액터를 갈아끼우는 것이 아니라 **값만 바꾼다**(D49) —
 * SkyLight / SkyAtmosphere / ExponentialHeightFog 는 월드에 하나만 유효하기 때문이다.
 */
USTRUCT(BlueprintType)
struct FTDZoneLightingSettings
{
	GENERATED_BODY()

	/** 태양 방향. Pitch 가 고도, Yaw 가 방위다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lighting")
	FRotator SunRotation = FRotator(-45.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lighting")
	FLinearColor SunColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lighting", meta = (ClampMin = "0.0"))
	float SunIntensity = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lighting", meta = (ClampMin = "0.0"))
	float SkyLightIntensity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lighting|Fog", meta = (ClampMin = "0.0"))
	float FogDensity = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lighting|Fog")
	FLinearColor FogColor = FLinearColor(0.5f, 0.6f, 0.7f);
};

/**
 * 한 존의 표현(Presentation) 전부. **클라이언트 전용이다.**
 *
 * 서버가 판정에 쓰는 값(입장 레벨·안전지대·부활 위치)은 여기 두지 않는다 —
 * 그것들은 DT_ZoneEnvironment 의 열로 직접 두어, 데디케이티드 서버가 BGM 이나
 * 텍스처를 메모리에 올리지 않고도 판정할 수 있게 한다(D5).
 *
 * ── 기본값 규칙 ──
 * 항목마다 성격이 다르다.
 *
 *   카메라·라이팅·BGM   기본값이 있다.  bOverride_ 를 켜야 이 에셋의 값이 쓰인다.
 *                       끄면 UTDZoneSettings 의 기본값이 쓰인다.
 *   환경음·미니맵       기본값이 없다.  비어 있으면 그냥 없는 것이다.
 *
 * bOverride_ 플래그를 두는 이유는 "지정하지 않음"과 "0(또는 무음)으로 지정"을
 * 구분하기 위해서다. 예를 들어 보스 등장 직전을 **의도적으로 무음**으로 하려면
 * bOverride_BGM 을 켜고 BGM 을 비워두면 된다.
 */
UCLASS(BlueprintType)
class TD_PROJECT_API UTDZoneEnvironmentData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ── 카메라 ────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera",
		meta = (InlineEditConditionToggle))
	bool bOverride_Camera = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera",
		meta = (EditCondition = "bOverride_Camera"))
	FTDZoneCameraSettings Camera;

	// ── 라이팅 ────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lighting",
		meta = (InlineEditConditionToggle))
	bool bOverride_Lighting = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lighting",
		meta = (EditCondition = "bOverride_Lighting"))
	FTDZoneLightingSettings Lighting;

	// ── 사운드 ────────────────────────────────────────────

	/**
	 * 켜면 아래 BGM 이 쓰인다. **비워두면 무음이 된다** — 그것이 이 플래그가 있는 이유다.
	 * 끄면 기본 BGM 이 계속 흐른다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound",
		meta = (InlineEditConditionToggle))
	bool bOverride_BGM = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound",
		meta = (EditCondition = "bOverride_BGM"))
	TSoftObjectPtr<USoundBase> BGM;

	/**
	 * 환경음(바람·물소리 등). 기본값이 없으므로 플래그도 없다 —
	 * 비어 있으면 그냥 환경음이 없는 존이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TSoftObjectPtr<USoundBase> AmbientSound;

	// ── UI ────────────────────────────────────────────────

	/** 미니맵이 생기면 쓴다. 비어 있으면 미니맵을 그리지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSoftObjectPtr<UTexture2D> MinimapTexture;
};
