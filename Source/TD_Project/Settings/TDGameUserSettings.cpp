#include "Settings/TDGameUserSettings.h"

#include "Algo/Reverse.h"
#include "AudioModulationStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "SoundControlBus.h"
#include "Settings/TDAudioSettings.h"

UTDGameUserSettings::UTDGameUserSettings()
{
	// 생성자에서 채워두지 않으면 ini 에 값이 없는 첫 실행에서 전부 0 이 된다.
	// SetToDefaults 는 "기본값으로 되돌리기"를 눌렀을 때만 불리므로 여기서도 채운다.
	SetToDefaults();
}

UTDGameUserSettings* UTDGameUserSettings::Get()
{
	return GEngine != nullptr
		? Cast<UTDGameUserSettings>(GEngine->GetGameUserSettings())
		: nullptr;
}

void UTDGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	MasterVolume = 1.f;
	BGMVolume = 0.7f;
	SFXVolume = 1.f;
	UIVolume = 1.f;
	bMuteWhenUnfocused = true;

	UIScale = 1.f;
	bShowDamageNumbers = true;
	bShowFPS = false;
	bShowPing = false;

	MaxVisibleCharacters = 0;   // 제한 없음
	CameraShakeScale = 1.f;

	ColorDeficiency = ETDColorDeficiency::None;
	ColorDeficiencySeverity = 1.f;
}

// ── 오디오 ────────────────────────────────────────────────

void UTDGameUserSettings::SetMasterVolume(float InValue)
{
	MasterVolume = FMath::Clamp(InValue, 0.f, 1.f);
	ApplyAudioSettings();
}

void UTDGameUserSettings::SetBGMVolume(float InValue)
{
	BGMVolume = FMath::Clamp(InValue, 0.f, 1.f);
	ApplyAudioSettings();
}

void UTDGameUserSettings::SetSFXVolume(float InValue)
{
	SFXVolume = FMath::Clamp(InValue, 0.f, 1.f);
	ApplyAudioSettings();
}

void UTDGameUserSettings::SetUIVolume(float InValue)
{
	UIVolume = FMath::Clamp(InValue, 0.f, 1.f);
	ApplyAudioSettings();
}

void UTDGameUserSettings::SetMuteWhenUnfocused(bool bInValue)
{
	bMuteWhenUnfocused = bInValue;
	ApplyUnfocusedVolume();
}

// ── 화면 ──────────────────────────────────────────────────

TArray<int32> UTDGameUserSettings::GetFrameRateLimitOptions()
{
	// 0 = 제한 없음. UI 는 이 값을 "무제한"으로 표시하면 된다.
	return { 30, 60, 120, 144, 0 };
}

TArray<FIntPoint> UTDGameUserSettings::GetSupportedResolutions()
{
	TArray<FIntPoint> Resolutions;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);

	// 큰 것부터 내려오게 뒤집는다. 엔진은 작은 것부터 주는데, 대개 위쪽에서 고른다.
	Algo::Reverse(Resolutions);

	if (Resolutions.Num() == 0)
	{
		// 전체화면 목록을 못 얻는 환경(일부 창모드 전용 설정)이 있다.
		// 빈 드롭다운을 내놓는 것보다 흔한 해상도라도 보이는 편이 낫다.
		UE_LOG(LogTemp, Warning,
			TEXT("지원 해상도 목록이 비었다. 기본 목록으로 대체한다."));

		Resolutions = { FIntPoint(1920, 1080), FIntPoint(1600, 900), FIntPoint(1280, 720) };
	}

	return Resolutions;
}

TArray<FString> UTDGameUserSettings::GetSupportedResolutionLabels()
{
	const TArray<FIntPoint> Resolutions = GetSupportedResolutions();

	TArray<FString> Labels;
	Labels.Reserve(Resolutions.Num());

	for (const FIntPoint& Resolution : Resolutions)
	{
		Labels.Add(FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y));
	}

	return Labels;
}

TArray<FString> UTDGameUserSettings::GetFrameRateLimitLabels()
{
	const TArray<int32> Limits = GetFrameRateLimitOptions();

	TArray<FString> Labels;
	Labels.Reserve(Limits.Num());

	for (const int32 Limit : Limits)
	{
		Labels.Add(Limit <= 0
			? NSLOCTEXT("TDSettings", "FrameRateUnlimited", "무제한").ToString()
			: FString::Printf(TEXT("%d"), Limit));
	}

	return Labels;
}

TArray<FText> UTDGameUserSettings::GetQualityPresetNames()
{
	// 인덱스가 곧 SetShadowQuality 등에 넣을 값이다(0~3).
	return {
		NSLOCTEXT("TDSettings", "QualityLow", "낮음"),
		NSLOCTEXT("TDSettings", "QualityMedium", "중간"),
		NSLOCTEXT("TDSettings", "QualityHigh", "높음"),
		NSLOCTEXT("TDSettings", "QualityEpic", "에픽")
	};
}

void UTDGameUserSettings::RevertToSaved()
{
	// ini 를 다시 읽는다. 적용하지 않은 변경은 여기서 버려진다.
	LoadSettings(/*bForceReload=*/ true);

	// 값을 되읽는 것만으로는 소리가 돌아오지 않는다. 버스에 다시 밀어넣어야 한다.
	ApplyAllSettings();
}

void UTDGameUserSettings::SetFrameRateLimitPreset(int32 InLimit)
{
	// 음수는 부모가 해석하지 못한다. 0 미만은 전부 "제한 없음"으로 본다.
	SetFrameRateLimit(InLimit > 0 ? static_cast<float>(InLimit) : 0.f);
}

// ── UI ────────────────────────────────────────────────────

void UTDGameUserSettings::SetUIScale(float InValue)
{
	// 0.5 미만이면 글자를 읽을 수 없고, 2.0 을 넘으면 창 밖으로 나간다.
	UIScale = FMath::Clamp(InValue, 0.5f, 2.f);
	ApplyUIScale();
}

void UTDGameUserSettings::SetShowDamageNumbers(bool bInValue)
{
	bShowDamageNumbers = bInValue;
}

void UTDGameUserSettings::SetShowFPS(bool bInValue)
{
	bShowFPS = bInValue;
}

void UTDGameUserSettings::SetShowPing(bool bInValue)
{
	bShowPing = bInValue;
}

// ── 게임플레이 ────────────────────────────────────────────

void UTDGameUserSettings::SetMaxVisibleCharacters(int32 InValue)
{
	MaxVisibleCharacters = FMath::Max(0, InValue);
}

void UTDGameUserSettings::SetCameraShakeScale(float InValue)
{
	CameraShakeScale = FMath::Clamp(InValue, 0.f, 1.f);
}

// ── 접근성 ────────────────────────────────────────────────

void UTDGameUserSettings::SetColorDeficiency(ETDColorDeficiency InValue)
{
	ColorDeficiency = InValue;
	ApplyColorDeficiency();
}

void UTDGameUserSettings::SetColorDeficiencySeverity(float InValue)
{
	ColorDeficiencySeverity = FMath::Clamp(InValue, 0.f, 1.f);
	ApplyColorDeficiency();
}

// ── 적용 ──────────────────────────────────────────────────

void UTDGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();

	ApplyAudioSettings();
	ApplyUnfocusedVolume();
	ApplyUIScale();
	ApplyColorDeficiency();
}

void UTDGameUserSettings::ApplyAllSettings()
{
	ApplyAudioSettings();
	ApplyUnfocusedVolume();
	ApplyUIScale();
	ApplyColorDeficiency();
}

UWorld* UTDGameUserSettings::GetAudioWorld() const
{
	if (GEngine == nullptr)
	{
		return nullptr;
	}

	// 게임 월드를 찾는다. 에디터 월드에는 오디오 디바이스가 다르게 붙어 있어
	// PIE 에서 엉뚱한 쪽에 값을 밀어넣지 않도록 종류를 확인한다.
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
		{
			if (Context.World() != nullptr)
			{
				return Context.World();
			}
		}
	}

	return nullptr;
}

void UTDGameUserSettings::ApplyAudioSettings()
{
	const UTDAudioSettings* AudioSettings = GetDefault<UTDAudioSettings>();
	if (AudioSettings == nullptr)
	{
		return;
	}

	UWorld* World = GetAudioWorld();
	if (World == nullptr)
	{
		// 아직 월드가 없는 시점(엔진 초기화 중)에 불릴 수 있다.
		// 저장된 값은 그대로 남아 있으므로, 월드가 생긴 뒤 ApplyAllSettings 로 다시 반영한다.
		return;
	}

	// 지정된 버스만 값을 보낸다. 사운드 담당이 아직 안 만든 버스가 있어도
	// 나머지는 정상 동작해야 하므로 하나씩 확인한다.
	const TPair<TSoftObjectPtr<USoundControlBus>, float> BusValues[] =
	{
		{ AudioSettings->MasterVolumeBus, MasterVolume },
		{ AudioSettings->BGMVolumeBus,    BGMVolume },
		{ AudioSettings->SFXVolumeBus,    SFXVolume },
		{ AudioSettings->UIVolumeBus,     UIVolume },
	};

	int32 AppliedCount = 0;

	for (const TPair<TSoftObjectPtr<USoundControlBus>, float>& Entry : BusValues)
	{
		USoundControlBus* Bus = Entry.Key.LoadSynchronous();
		if (Bus == nullptr)
		{
			continue;
		}

		// 전역 믹스에 값을 하나 얹는다. 항상 켜져 있어야 하는 버스에 쓰라고
		// 엔진이 안내하는 경로이며, 볼륨 설정이 정확히 그 경우다.
		// SoundMix 와 달리 스택이 아니라서 여러 번 불러도 누적되지 않는다.
		UAudioModulationStatics::SetGlobalBusMixValue(
			World, Bus, Entry.Value, AudioSettings->VolumeFadeTime);

		++AppliedCount;
	}

	if (AppliedCount == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("오디오 옵션: Control Bus 가 하나도 지정되지 않았다. "
			     "프로젝트 세팅 > TD > Audio 를 확인할 것. 볼륨 조절이 동작하지 않는다."));
	}
}

void UTDGameUserSettings::ApplyUnfocusedVolume()
{
	// 포커스가 떠난 것을 감지하고 이 배율을 곱하는 일은 엔진이 매 프레임 처리한다.
	// 우리는 값만 정해주면 되고, 창 이벤트를 구독할 필요가 없다.
	FApp::SetUnfocusedVolumeMultiplier(bMuteWhenUnfocused ? 0.f : 1.f);
}

void UTDGameUserSettings::ApplyUIScale()
{
	if (FSlateApplication::IsInitialized())
	{
		// 위젯을 하나씩 건드리는 대신 슬레이트 전체 배율을 바꾼다.
		// UI 담당이 위젯에 따로 손댈 것이 없다.
		FSlateApplication::Get().SetApplicationScale(UIScale);
	}
}

void UTDGameUserSettings::ApplyColorDeficiency()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	EColorVisionDeficiency EngineType = EColorVisionDeficiency::NormalVision;
	switch (ColorDeficiency)
	{
	case ETDColorDeficiency::Deuteranope:
		EngineType = EColorVisionDeficiency::Deuteranope;
		break;
	case ETDColorDeficiency::Protanope:
		EngineType = EColorVisionDeficiency::Protanope;
		break;
	case ETDColorDeficiency::Tritanope:
		EngineType = EColorVisionDeficiency::Tritanope;
		break;
	default:
		break;
	}

	// None 이면 종류를 NormalVision 으로 되돌린다. Severity 만 0 으로 두면
	// 다음에 켤 때 옛 종류가 남아 엉뚱한 보정이 걸린다.
	const bool bEnabled = ColorDeficiency != ETDColorDeficiency::None;

	// 세 번째 인자(CorrectDeficiency)가 핵심이다. false 면 "색약인 사람에게 어떻게
	// 보이는지"를 흉내내는 **시뮬레이션**이 되어, 정작 색약인 사람에게는 도움이 안 된다.
	// 네 번째(ShowCorrectionWithDeficiency)는 개발자가 둘을 비교하는 용도라 끈다.
	UWidgetBlueprintLibrary::SetColorVisionDeficiencyType(
		EngineType,
		bEnabled ? ColorDeficiencySeverity : 0.f,
		/*CorrectDeficiency=*/ bEnabled,
		/*ShowCorrectionWithDeficiency=*/ false);
}
