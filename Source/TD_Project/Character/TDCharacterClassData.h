#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TDCharacterClassData.generated.h"

class UPaperZDAnimInstance;
class UTexture2D;

/**
 * 직업 하나의 **그림**을 모아둔 에셋. DT_CharacterClass 의 행이 이것을 가리킨다.
 *
 * 테이블에 텍스처를 직접 넣지 않고 한 겹 나눈 이유는 크기 때문이다. 전신 그림은
 * 캐릭터 선택 화면에서만 쓰는데 보통 1~2MB 다. 테이블에 박아두면 파티창이 아이콘
 * 하나 읽으려고 테이블을 여는 순간 세 직업의 전신 그림까지 딸려 올 여지가 생긴다.
 * DT_ZoneEnvironment 와 UTDZoneEnvironmentData 를 나눈 것과 같은 이유다.
 *
 * 세 항목 모두 TSoftObjectPtr 이라, 이 에셋을 들고 있는 것만으로는 텍스처가
 * 메모리에 올라오지 않는다(D5). 화면이 필요한 것만 LoadSynchronous 하거나
 * 비동기로 요청하면 된다 — Image 위젯의 Brush 에 Soft 참조를 바로 물릴 수도 있다.
 *
 * "직업 = 캐릭터" 이므로 나중에 스켈레탈 메시나 애님 블루프린트처럼 그 직업을
 * 이루는 다른 것들도 여기 들어올 자리다.
 */
UCLASS(BlueprintType)
class TD_PROJECT_API UTDCharacterClassData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ── 그림 ──────────────────────────────────────────────
	// 셋 다 같은 직업을 그리지만 쓰이는 자리가 달라 크기와 구도가 다르다.
	// 하나로 합쳐 쓰면 파티창에 전신 그림을 욱여넣게 된다.

	/** 아이콘. 작고 가장 자주 쓴다 — 파티 목록, HUD, 캐릭터 슬롯의 직업 표시. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portrait")
	TSoftObjectPtr<UTexture2D> Icon;

	/** 얼굴만. 캐릭터 선택 목록의 각 줄, 대화창처럼 인물을 가리켜야 하는 자리. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portrait")
	TSoftObjectPtr<UTexture2D> Portrait;

	/** 전신. 캐릭터 선택 화면에서 크게 보여주는 그림. 셋 중 가장 무겁다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portrait")
	TSoftObjectPtr<UTexture2D> FullBody;

	// ── 외형 ──────────────────────────────────────────────

	/**
	 * 월드에 보이는 캐릭터의 PaperZD 애니메이션 BP(ABP_Black 등).
	 *
	 * ATDPlayerCharacter 가 PlayerState 가 붙는 순간(서버 PossessedBy · 클라 OnRep_PlayerState)과
	 * 직업이 바뀔 때 넣는다. BeginPlay 에서 넣으면 서버에서는 아직 PlayerState 가 없어(스폰이 빙의보다
	 * 먼저다) 기본 외형으로 남는다 — 마법사가 가끔 전사로 보이던 버그(2026-09-16).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TSoftClassPtr<UPaperZDAnimInstance> AnimInstanceClass;
};
