#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "World/TDZoneEnvironmentData.h"
#include "TDZoneSettings.generated.h"

class USoundBase;
class UDataTable;

/**
 * 존이 값을 지정하지 않았을 때 쓰이는 기본값. 프로젝트 세팅 > TD > Zone 에 나타난다.
 *
 * 에셋이 아니라 **구조체를 직접** 담는 이유는 셋이다.
 *   · 로드가 필요 없다 — 존을 옮길 때마다 기본 에셋을 읽지 않아도 된다
 *   · ini 에 텍스트로 저장되므로 git diff 가 보인다(D61 의 대가를 여기서는 치르지 않는다)
 *   · 기획이 프로젝트 세팅에서 바로 만질 수 있다
 *
 * 대부분의 존은 아무것도 오버라이드하지 않고 이 값으로 돈다. 특별한 존만
 * UTDZoneEnvironmentData 에서 필요한 항목만 켠다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "TD Zone"))
class TD_PROJECT_API UTDZoneSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("TD"); }

	/** DT_ZoneEnvironment. 존 정보를 여기서 읽는다. 경로를 코드에 박지 않기 위한 자리다(D45). */
	UPROPERTY(config, EditAnywhere, Category = "Zone")
	TSoftObjectPtr<UDataTable> ZoneEnvironmentTable;

	/** 어느 존에도 속하지 않을 때(테이블에 행이 없을 때) 쓰이는 카메라. */
	UPROPERTY(config, EditAnywhere, Category = "Default")
	FTDZoneCameraSettings DefaultCamera;

	UPROPERTY(config, EditAnywhere, Category = "Default")
	FTDZoneLightingSettings DefaultLighting;

	/**
	 * 존이 BGM 을 지정하지 않았을 때 흐르는 곡.
	 *
	 * 비워두면 기본 BGM 없이 시작한다. 존마다 곡을 다 채울 계획이면 비워두는 편이
	 * 낫다 — 빠뜨린 존이 있을 때 엉뚱한 곡이 흐르는 대신 조용해서 바로 알아챌 수 있다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Default")
	TSoftObjectPtr<USoundBase> DefaultBGM;

	/**
	 * 세이브에 마지막 존이 없는 신규 캐릭터가 시작할 존.
	 *
	 * ATDGameMode 의 DefaultSpawnZoneTag 와 짝이며, 그쪽이 스폰 지점을 고를 때 쓴다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Default")
	FGameplayTag DefaultStartZone;
};
