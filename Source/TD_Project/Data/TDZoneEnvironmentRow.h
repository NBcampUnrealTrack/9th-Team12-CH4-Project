#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "TDZoneEnvironmentRow.generated.h"

class UTDZoneEnvironmentData;

/**
 * DT_ZoneEnvironment 의 행. 존 하나의 정의다.
 *
 * ── 왜 열이 나뉘어 있는가 ──
 * 표현(BGM·라이팅·카메라)은 DataAsset 한 칸으로 몰아넣고, **서버가 판정에 쓰는 값만**
 * 열로 꺼내 두었다.
 *
 * 전부 DataAsset 에 넣으면 데디케이티드 서버가 입장 레벨 하나를 확인하려고 BGM 과
 * 텍스처까지 메모리에 올려야 한다. 반대로 전부 열로 펼치면 라이팅 파라미터가 늘 때마다
 * 시트 스키마가 바뀐다.
 *
 * 스폰 좌표는 여기 없다. PlayerStart 의 태그에 존 태그 문자열을 그대로 쓰므로(D59)
 * ZoneId 자체가 스폰 지점 키다. 열을 따로 두면 같은 값을 두 곳에 적게 된다.
 */
USTRUCT(BlueprintType)
struct FTDZoneEnvironmentRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Zone.Region1.Field01 형태. RowName 과 별개로 두는 이유는 태그 계층 조회 때문이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	FGameplayTag ZoneId;

	/** UI 에 표시할 지역명. 서버 로그에도 쓰인다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	FText DisplayName;

	// ── 서버 판정 ─────────────────────────────────────────
	// 클라이언트가 보내는 것은 "가고 싶다"는 의도뿐이고, 갈 수 있는지는 서버가 정한다.

	/** 입장에 필요한 레벨. 0 이면 제한 없음. 서버가 텔레포트 요청을 거부하는 근거다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Rules", meta = (ClampMin = "0"))
	int32 RequiredLevel = 0;

	/**
	 * 안전지대. 켜면 전투가 일어나지 않는다.
	 *
	 * 판정은 전투 쪽이 하고, 이 값은 그 근거만 제공한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Rules")
	bool bIsSafeZone = false;

	/**
	 * 여기서 죽으면 어느 존으로 돌아가는가. 비어 있으면 기본 시작 존.
	 *
	 * 지역2 에서 죽었는데 지역1 마을로 돌아가면 곤란하므로 존마다 지정할 수 있게 둔다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Rules")
	FGameplayTag RespawnZoneId;

	/**
	 * 권장 레벨. 막지 않고 알려만 준다. UI 가 "권장 25레벨" 로 표시한다.
	 *
	 * RequiredLevel 과 다르다 — 그쪽은 서버가 입장을 거부하는 값이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Rules", meta = (ClampMin = "0"))
	int32 RecommendedLevel = 0;

	// ── 클라이언트 표현 ───────────────────────────────────

	/**
	 * 카메라·라이팅·BGM·환경음·미니맵 묶음. 비워두면 전부 UTDZoneSettings 의 기본값으로 돈다.
	 *
	 * **환경음 하나가 아니라 표현 전체다.** 라이팅 6개 + 카메라 4개를 열로 펼치면
	 * 시트가 15열을 넘고, 파라미터가 늘 때마다 스키마가 바뀐다.
	 *
	 * 소프트 참조라 서버는 이 에셋을 읽지 않는다(D5).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Presentation")
	TSoftObjectPtr<UTDZoneEnvironmentData> EnvironmentData;
};
