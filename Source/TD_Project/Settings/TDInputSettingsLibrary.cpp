#include "Settings/TDInputSettingsLibrary.h"

#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace
{
	/**
	 * 사용자 설정 객체를 가져온다. 없으면 경고를 남긴다.
	 *
	 * 없는 경우는 둘뿐이고 **둘 다 조용히 실패**하므로 여기서 잡아준다.
	 *   · DefaultInput.ini 의 bEnableUserSettings 가 꺼져 있다
	 *   · 로컬 플레이어가 아니다(데디케이티드 서버의 원격 컨트롤러 등)
	 */
	UEnhancedInputUserSettings* GetUserSettings(const APlayerController* Player)
	{
		if (Player == nullptr)
		{
			// 조용히 넘기면 "목록이 그냥 비어 있는" 것과 구분되지 않는다.
			// 대개 위젯을 만들 때 Owning Player 를 넘기지 않아 GetOwningPlayer 가 null 인 경우다.
			UE_LOG(LogTemp, Warning,
				TEXT("키 설정: PlayerController 가 null 이다. "
				     "위젯을 Create Widget 으로 만들 때 Owning Player 를 지정했는지 확인할 것."));
			return nullptr;
		}

		const ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
		if (LocalPlayer == nullptr)
		{
			// 서버가 들고 있는 원격 컨트롤러다. 키 설정은 각 클라이언트의 것이라 여기 없다.
			return nullptr;
		}

		UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

		if (Subsystem == nullptr)
		{
			return nullptr;
		}

		UEnhancedInputUserSettings* Settings = Subsystem->GetUserSettings();
		if (Settings == nullptr)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("키 설정: UEnhancedInputUserSettings 가 없다. "
				     "DefaultInput.ini 의 [EnhancedInputDeveloperSettings] bEnableUserSettings=True 를 확인할 것."));
		}

		return Settings;
	}
}

TArray<FTDKeyMappingRow> UTDInputSettingsLibrary::GetKeyMappings(APlayerController* Player)
{
	TArray<FTDKeyMappingRow> Rows;

	const UEnhancedInputUserSettings* Settings = GetUserSettings(Player);
	if (Settings == nullptr)
	{
		return Rows;
	}

	// GetCurrentKeyProfile 은 5.6 에서 폐기됐다. 다음 릴리스에서 컴파일이 깨진다.
	const UEnhancedPlayerMappableKeyProfile* Profile = Settings->GetActiveKeyProfile();
	if (Profile == nullptr)
	{
		return Rows;
	}

	// 한 이름에 여러 슬롯(기본키·보조키)이 있을 수 있다. 지금은 첫 번째만 보여준다 —
	// 보조키까지 노출하면 UI 가 두 배로 복잡해지고, 아직 그 요구가 없다.
	for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
	{
		for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
		{
			FTDKeyMappingRow& Row = Rows.AddDefaulted_GetRef();
			Row.MappingName = Mapping.GetMappingName();
			Row.DisplayName = Mapping.GetDisplayName();
			Row.DisplayCategory = Mapping.GetDisplayCategory();
			Row.CurrentKey = Mapping.GetCurrentKey();
			break;
		}
	}

	if (Rows.IsEmpty())
	{
		// 목록이 비었다는 것은 리매핑 가능한 액션이 하나도 없다는 뜻이다.
		UE_LOG(LogTemp, Warning,
			TEXT("키 설정: 리매핑 가능한 액션이 없다. "
			     "IA_* 에셋의 Player Mappable Key Settings 에 Name 을 지정해야 목록에 나온다."));
	}

	return Rows;
}

bool UTDInputSettingsLibrary::SetKeyMapping(APlayerController* Player, FName MappingName, FKey NewKey)
{
	UEnhancedInputUserSettings* Settings = GetUserSettings(Player);
	if (Settings == nullptr || MappingName.IsNone() || !NewKey.IsValid())
	{
		return false;
	}

	FMapPlayerKeyArgs Args;
	Args.MappingName = MappingName;
	Args.NewKey = NewKey;
	Args.Slot = EPlayerMappableKeySlot::First;

	// 실패 사유가 태그 컨테이너로 온다. 조용히 실패하지 않도록 그대로 로그에 남긴다.
	FGameplayTagContainer FailureReason;
	Settings->MapPlayerKey(Args, FailureReason);

	if (!FailureReason.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("키 설정 실패: '%s' → %s (%s)"),
			*MappingName.ToString(), *NewKey.ToString(), *FailureReason.ToString());
		return false;
	}

	// 즉시 저장한다. 게임을 끄기 전에 반영돼야 하고, 저장 자체는 엔진이 맡는다.
	Settings->AsyncSaveSettings();

	return true;
}

bool UTDInputSettingsLibrary::ResetKeyMappingsToDefault(APlayerController* Player)
{
	UEnhancedInputUserSettings* Settings = GetUserSettings(Player);
	if (Settings == nullptr)
	{
		return false;
	}

	// 태그를 받는 옛 버전은 5.6 에서 폐기됐다. FString 을 받는 쪽을 쓴다.
	FGameplayTagContainer FailureReason;
	Settings->ResetKeyProfileIdToDefault(Settings->GetActiveKeyProfileId(), FailureReason);

	if (!FailureReason.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("키 설정 초기화 실패: %s"), *FailureReason.ToString());
		return false;
	}

	Settings->AsyncSaveSettings();

	return true;
}
