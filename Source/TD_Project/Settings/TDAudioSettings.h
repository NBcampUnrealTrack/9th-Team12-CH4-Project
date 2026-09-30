#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TDAudioSettings.generated.h"

class USoundControlBus;

/**
 * 볼륨 조절에 쓰는 오디오 에셋을 지정하는 곳. 프로젝트 세팅 > TD > Audio 에 나타난다.
 *
 * 값(볼륨 0.7)이 아니라 **에셋 참조**만 둔다. 값은 사람마다 다르므로
 * UTDGameUserSettings 가 ini 에 저장하고, 여기 있는 것은 프로젝트 전체가 공유한다.
 *
 * 경로를 코드에 문자열로 박지 않는 이유는 D45 다. 에셋을 옮기거나 이름을 바꾸면
 * 조용히 nullptr 이 되고, 그때는 "소리가 안 줄어든다"는 증상만 남아 원인을 찾기 어렵다.
 * 여기 두면 에디터가 참조를 추적하므로 에셋을 옮겨도 링크가 유지된다.
 *
 * 에셋이 아직 없어도 동작에는 문제가 없다. 지정되지 않은 버스는 건너뛰고 경고만 남긴다.
 *
 * ── 사운드 담당에게 ──
 * 만들 에셋은 Control Bus 네 개뿐이다(Bus Mix 는 필요 없다 —
 * SetGlobalBusMixValue 가 버스마다 전역 믹스를 알아서 다룬다).
 *
 *   1. Content/Audio 에 Sound Control Bus 를 4개 만든다 (Master/BGM/SFX/UI)
 *      · Parameter 는 Volume 계열로 지정한다
 *   2. Submix 를 만들어 각 Submix 의 Output Volume Modulation 에 해당 버스를 건다
 *      · BGM/SFX/UI Submix 를 Master Submix 아래에 두면 Master 가 자연히 전체에 걸린다
 *   3. 사운드 에셋은 Submix 만 고르면 된다. 개별 사운드에 버스를 걸 필요는 없다
 *   4. 아래 네 칸에 버스를 지정한다
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "TD Audio"))
class TD_PROJECT_API UTDAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("TD"); }

	/**
	 * 전체 볼륨. 다른 버스와 곱해지는 것이 아니라 **Submix 계층에서 자연히 합쳐진다** —
	 * BGM/SFX/UI Submix 가 Master Submix 아래에 있으면 된다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Control Bus")
	TSoftObjectPtr<USoundControlBus> MasterVolumeBus;

	UPROPERTY(config, EditAnywhere, Category = "Control Bus")
	TSoftObjectPtr<USoundControlBus> BGMVolumeBus;

	UPROPERTY(config, EditAnywhere, Category = "Control Bus")
	TSoftObjectPtr<USoundControlBus> SFXVolumeBus;

	UPROPERTY(config, EditAnywhere, Category = "Control Bus")
	TSoftObjectPtr<USoundControlBus> UIVolumeBus;

	/**
	 * 볼륨을 바꿀 때의 페이드 시간(초).
	 *
	 * 0 이면 슬라이더를 드래그하는 동안 소리가 계단처럼 끊긴다.
	 * 너무 길면 슬라이더를 놓고도 한참 뒤에 반영돼 조작감이 나빠진다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Control Bus", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VolumeFadeTime = 0.1f;
};
