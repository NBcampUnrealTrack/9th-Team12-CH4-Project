#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "TDGameUserSettings.generated.h"

/**
 * 색약 보정 종류. 언리얼의 EColorVisionDeficiency 와 짝이지만 별도로 둔다 —
 * 그쪽은 렌더링 모듈 enum 이라 UI 에 그대로 노출하면 우리가 안 쓰는 값까지 딸려온다.
 */
UENUM(BlueprintType)
enum class ETDColorDeficiency : uint8
{
	None			UMETA(DisplayName = "사용 안 함"),
	Deuteranope		UMETA(DisplayName = "녹색약 (적록)"),
	Protanope		UMETA(DisplayName = "적색약 (적록)"),
	Tritanope		UMETA(DisplayName = "청색약")
};

/**
 * 이 게임의 모든 옵션 값. 화면·화질은 부모가 이미 갖고 있고, 여기서는 그 밖의 것을 더한다.
 *
 * UGameUserSettings 를 상속하는 이유는 저장 때문이다. 부모가 GameUserSettings.ini 로
 * 읽고 쓰는 일을 전부 처리하므로 우리는 UPROPERTY(config) 만 붙이면 된다.
 * 세이브 시스템과 무관하며, 계정이 아니라 **그 PC 에** 저장된다.
 *
 * 서버와도 무관하다. 여기 있는 값은 전부 클라이언트가 자기 화면과 소리를 어떻게 다룰지에
 * 대한 것이라 복제하지 않는다.
 *
 * ── UI 담당에게 ──
 * 옵션 창은 이 클래스만 알면 된다. 가져오는 법:
 *
 *     UTDGameUserSettings* Settings = UTDGameUserSettings::Get();
 *
 * 화면·화질은 부모 함수를 그대로 쓴다(SetScreenResolution, SetFullscreenMode,
 * SetShadowQuality, RunHardwareBenchmark 등). 여기 다시 만들지 않았다.
 *
 * 값을 바꾼 뒤에는 ApplySettings(false) 를 부른다. 저장까지 함께 이뤄진다.
 */
UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class TD_PROJECT_API UTDGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UTDGameUserSettings();

	/**
	 * 어디서든 이걸로 가져온다. 엔진이 만들어 둔 단일 인스턴스다.
	 *
	 * DisplayName 을 붙인 이유는 블루프린트 검색 때문이다. 함수 이름이 Get 이라
	 * 노드 이름도 "Get" 이 되는데, 그 단어로는 수백 개가 걸려 찾을 수 없다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Settings",
		meta = (DisplayName = "Get TD Game User Settings"))
	static UTDGameUserSettings* Get();

	// ── 오디오 ────────────────────────────────────────────
	// 전부 0~1. 슬라이더에 그대로 연결하면 된다.

	UFUNCTION(BlueprintPure, Category = "TD|Settings|Audio")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintCallable, Category = "TD|Settings|Audio")
	void SetMasterVolume(float InValue);

	UFUNCTION(BlueprintPure, Category = "TD|Settings|Audio")
	float GetBGMVolume() const { return BGMVolume; }

	UFUNCTION(BlueprintCallable, Category = "TD|Settings|Audio")
	void SetBGMVolume(float InValue);

	UFUNCTION(BlueprintPure, Category = "TD|Settings|Audio")
	float GetSFXVolume() const { return SFXVolume; }

	UFUNCTION(BlueprintCallable, Category = "TD|Settings|Audio")
	void SetSFXVolume(float InValue);

	UFUNCTION(BlueprintPure, Category = "TD|Settings|Audio")
	float GetUIVolume() const { return UIVolume; }

	UFUNCTION(BlueprintCallable, Category = "TD|Settings|Audio")
	void SetUIVolume(float InValue);

	/**
	 * 창이 뒤로 갔을 때 소리를 끌지 여부.
	 *
	 * 포커스가 떠난 것을 감지하는 일은 엔진이 이미 하고 있다. 우리는 그때 곱할
	 * 배율만 정해주면 되므로, 창 이벤트를 직접 구독할 필요가 없다.
	 *
	 * MORPG 라 창을 띄워두고 다른 일을 하는 경우가 잦다. 기본값은 끄기(true)다 —
	 * 배경에서 계속 소리가 나는 쪽이 더 성가시다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Settings|Audio")
	bool GetMuteWhenUnfocused() const { return bMuteWhenUnfocused; }

	UFUNCTION(BlueprintCallable, Category = "TD|Settings|Audio")
	void SetMuteWhenUnfocused(bool bInValue);

	// ── 화면 ──────────────────────────────────────────────
	// 해상도·창모드·화질은 부모 함수를 그대로 쓴다. 프레임 제한만 여기서 거든다 —
	// 부모의 SetFrameRateLimit 은 아무 실수나 받는데, 드롭다운에는 정해진 몇 개만 필요하다.

	/**
	 * 드롭다운에 그대로 쓸 수 있는 프레임 제한 목록. 0 은 "제한 없음"이다.
	 *
	 * 모니터 주사율은 넣지 않는다. VSync 를 켜면 그쪽이 알아서 맞추고,
	 * 목록에 넣으면 주사율이 다른 PC 마다 값이 달라져 저장된 설정이 어긋난다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Settings|Video")
	static TArray<int32> GetFrameRateLimitOptions();

	/** 목록에 없는 값이 들어와도 받는다 — 사용자가 직접 입력하는 칸을 열 수도 있어서다. */
	UFUNCTION(BlueprintCallable, Category = "TD|Settings|Video")
	void SetFrameRateLimitPreset(int32 InLimit);

	/**
	 * 이 모니터가 지원하는 해상도 목록. 드롭다운에 그대로 채운다.
	 *
	 * 엔진의 GetSupportedFullscreenResolutions 는 블루프린트에 열려 있지 않아 감싼다.
	 * 큰 것부터 내려오도록 정렬한다 — 대개 위쪽에서 고른다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Settings|Video")
	static TArray<FIntPoint> GetSupportedResolutions();

	/**
	 * 위 목록을 "1920 x 1080" 형태로 만든 것. **콤보박스에 그대로 넣는다.**
	 *
	 * 블루프린트에서 FIntPoint 를 문자열로 조립하려면 노드가 서너 개 필요하고,
	 * 이름이 겹치는 다른 Append 를 잡기도 쉬워 여기서 만들어 준다.
	 *
	 * GetSupportedResolutions 와 **순서가 같다.** 콤보박스에서 고른 인덱스로
	 * 그쪽 배열을 조회하면 실제 해상도가 나온다 — 문자열을 다시 숫자로 파싱하면
	 * 표기를 바꾸는 순간 깨진다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Settings|Video")
	static TArray<FString> GetSupportedResolutionLabels();

	/**
	 * 프레임 제한 라벨. 0 은 "무제한" 으로 바꿔 준다.
	 *
	 * GetFrameRateLimitOptions 와 순서가 같다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Settings|Video")
	static TArray<FString> GetFrameRateLimitLabels();

	/**
	 * 화질 단계 이름. 그림자·텍스처·안티앨리어싱·시야거리가 같은 목록을 쓴다.
	 *
	 * 배열 인덱스가 곧 SetShadowQuality 등에 넣을 값이다(0~3).
	 * 문자열을 BP 에 하드코딩하지 않는 이유는 현지화 때문이다 — 그러면 나중에
	 * 위젯을 뒤져 가며 고쳐야 한다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Settings|Video")
	static TArray<FText> GetQualityPresetNames();

	/**
	 * 저장된 값으로 되돌린다. **"취소" 버튼용이다.**
	 *
	 * ini 에서 다시 읽으므로 적용하지 않은 변경은 버려진다. 볼륨처럼 즉시
	 * 반영되던 것도 원래 값으로 돌아온다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TD|Settings")
	void RevertToSaved();

	// ── UI ────────────────────────────────────────────────

	/**
	 * UI 전체 배율. 4K 에서 글자가 깨알같아지는 것을 막는다.
	 *
	 * 위젯마다 크기를 조절하는 것이 아니라 엔진의 ApplicationScale 을 건드리므로,
	 * UI 담당이 위젯에 따로 손댈 것은 없다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Settings|UI")
	float GetUIScale() const { return UIScale; }

	UFUNCTION(BlueprintCallable, Category = "TD|Settings|UI")
	void SetUIScale(float InValue);

	UFUNCTION(BlueprintPure, Category = "TD|Settings|UI")
	bool GetShowDamageNumbers() const { return bShowDamageNumbers; }

	UFUNCTION(BlueprintCallable, Category = "TD|Settings|UI")
	void SetShowDamageNumbers(bool bInValue);

	UFUNCTION(BlueprintPure, Category = "TD|Settings|UI")
	bool GetShowFPS() const { return bShowFPS; }

	UFUNCTION(BlueprintCallable, Category = "TD|Settings|UI")
	void SetShowFPS(bool bInValue);

	UFUNCTION(BlueprintPure, Category = "TD|Settings|UI")
	bool GetShowPing() const { return bShowPing; }

	UFUNCTION(BlueprintCallable, Category = "TD|Settings|UI")
	void SetShowPing(bool bInValue);

	// ── 게임플레이 ────────────────────────────────────────

	/**
	 * 화면에 그릴 다른 플레이어 수의 상한. 0 이면 제한 없음.
	 *
	 * 복제를 끄는 것이 아니라 **그리기만 멈춘다.** 서버는 여전히 전부 보내고 있고
	 * 판정도 정상이다. 마을에 30명이 모였을 때 저사양 PC 를 위한 장치다.
	 *
	 * 실제로 누구를 숨길지는 이 값을 읽는 쪽이 정한다(가까운 순 등).
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Settings|Gameplay")
	int32 GetMaxVisibleCharacters() const { return MaxVisibleCharacters; }

	UFUNCTION(BlueprintCallable, Category = "TD|Settings|Gameplay")
	void SetMaxVisibleCharacters(int32 InValue);

	/** 카메라 흔들림 배율. 0 이면 흔들리지 않는다(멀미 대응). */
	UFUNCTION(BlueprintPure, Category = "TD|Settings|Gameplay")
	float GetCameraShakeScale() const { return CameraShakeScale; }

	UFUNCTION(BlueprintCallable, Category = "TD|Settings|Gameplay")
	void SetCameraShakeScale(float InValue);

	// ── 접근성 ────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "TD|Settings|Accessibility")
	ETDColorDeficiency GetColorDeficiency() const { return ColorDeficiency; }

	UFUNCTION(BlueprintCallable, Category = "TD|Settings|Accessibility")
	void SetColorDeficiency(ETDColorDeficiency InValue);

	/** 보정 강도 0~1. 종류가 None 이면 쓰이지 않는다. */
	UFUNCTION(BlueprintPure, Category = "TD|Settings|Accessibility")
	float GetColorDeficiencySeverity() const { return ColorDeficiencySeverity; }

	UFUNCTION(BlueprintCallable, Category = "TD|Settings|Accessibility")
	void SetColorDeficiencySeverity(float InValue);

	// ── 적용 ──────────────────────────────────────────────

	/**
	 * 부모가 화면·화질을 적용할 때 함께 불린다. 우리 값도 여기서 반영한다.
	 *
	 * UI 는 이 함수를 직접 부르지 않는다 — ApplySettings() 를 부르면 엔진이 부른다.
	 */
	virtual void ApplyNonResolutionSettings() override;

	virtual void SetToDefaults() override;

	/**
	 * 게임 시작 시 한 번 불러 저장된 값을 실제로 반영한다.
	 *
	 * ini 를 읽는 것만으로는 소리가 줄지 않는다. 값이 메모리에 올라올 뿐이고
	 * 버스에 밀어넣는 것은 별개의 일이기 때문이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TD|Settings")
	void ApplyAllSettings();

private:
	/** 볼륨 값을 Control Bus 로 보낸다. 에셋이 지정되지 않았으면 경고만 남기고 넘어간다. */
	void ApplyAudioSettings();

	/** 창이 뒤로 갔을 때 곱할 배율을 엔진에 알린다. 감지는 엔진이 한다. */
	void ApplyUnfocusedVolume();

	void ApplyUIScale();
	void ApplyColorDeficiency();

	/** 오디오 적용에는 World 가 필요하다. 어느 것이든 게임 월드 하나면 된다. */
	UWorld* GetAudioWorld() const;

	// ── 저장되는 값 ───────────────────────────────────────
	// UPROPERTY(config) 가 붙은 것만 GameUserSettings.ini 에 남는다.

	UPROPERTY(config)
	float MasterVolume;

	UPROPERTY(config)
	float BGMVolume;

	UPROPERTY(config)
	float SFXVolume;

	UPROPERTY(config)
	float UIVolume;

	UPROPERTY(config)
	bool bMuteWhenUnfocused;

	UPROPERTY(config)
	float UIScale;

	UPROPERTY(config)
	bool bShowDamageNumbers;

	UPROPERTY(config)
	bool bShowFPS;

	UPROPERTY(config)
	bool bShowPing;

	UPROPERTY(config)
	int32 MaxVisibleCharacters;

	UPROPERTY(config)
	float CameraShakeScale;

	UPROPERTY(config)
	ETDColorDeficiency ColorDeficiency;

	UPROPERTY(config)
	float ColorDeficiencySeverity;
};
