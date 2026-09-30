#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TDInputSettingsLibrary.generated.h"

class APlayerController;

/** 리매핑 화면 한 줄. UI 가 목록을 그릴 때 필요한 것만 담는다. */
USTRUCT(BlueprintType)
struct FTDKeyMappingRow
{
	GENERATED_BODY()

	/** 코드가 쓰는 식별자. IA 에셋의 Player Mappable Key Settings > Name 이다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Input")
	FName MappingName;

	/** 화면에 보이는 이름. 같은 곳의 Display Name 이다. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Input")
	FText DisplayName;

	/**
	 * 설정 화면에서 묶을 그룹. IA 의 Display Category 다.
	 *
	 * 비어 있을 수 있다 — UI 는 그때 "기타" 같은 기본 그룹에 넣거나
	 * 그룹 없이 한 줄로 나열하면 된다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Input")
	FText DisplayCategory;

	/** 지금 지정된 키. */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Input")
	FKey CurrentKey;
};

/**
 * 키 리매핑 래퍼. **UI 담당이 쓸 창구다.**
 *
 * Enhanced Input 의 UEnhancedInputUserSettings 가 실제 일을 다 한다 —
 * 매핑 저장도 `Saved/SaveGames/EnhancedInputUserSettings.sav` 에 엔진이 알아서 한다.
 * 여기서는 UI 가 쓰기 편한 모양으로 감싸기만 한다.
 *
 * ── 선행 조건 ──
 * 리매핑하려는 IA 에셋마다 **Player Mappable Key Settings** 를 설정해야 한다.
 * 설정하지 않은 액션은 목록에 아예 나오지 않는다.
 *
 *   IA_Move 를 열고
 *     Setting Behavior = Inherit Settings (또는 Override)
 *     Name         = "Move"      ← MappingName 이 된다
 *     Display Name = "이동"       ← 화면에 보이는 이름
 *
 * 그리고 `DefaultInput.ini` 의 `bEnableUserSettings=True` 가 있어야 한다.
 * 없으면 사용자 설정 객체가 아예 만들어지지 않아 전부 조용히 실패한다.
 *
 * ── 저장 위치 ──
 * 키 매핑은 **그 PC 에** 저장된다. 캐릭터를 바꿔도 손가락 위치는 그대로여야 하기
 * 때문이며, 계정 세이브(FTDPlayerSaveData)와는 무관하다. 퀵슬롯 **배치**는 반대로
 * 캐릭터별이라 세이브에 들어간다.
 */
UCLASS()
class TD_PROJECT_API UTDInputSettingsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 리매핑 가능한 키 목록. 설정 화면이 이걸로 줄을 그린다.
	 *
	 * 목록이 비어 있으면 IA 에셋에 Player Mappable Key Settings 가 없거나
	 * bEnableUserSettings 가 꺼진 것이다. 둘 다 조용히 실패하므로 경고를 남긴다.
	 */
	// WorldContext 메타를 붙이지 않는다. 그것은 "컴파일러가 알아서 채우니 핀을 숨겨라"
	// 라는 뜻이라, 실제로 넘겨야 하는 인자에 붙이면 **핀이 사라져 연결할 수 없게 된다.**
	UFUNCTION(BlueprintPure, Category = "TD|Input")
	static TArray<FTDKeyMappingRow> GetKeyMappings(APlayerController* Player);

	/**
	 * 키를 바꾸고 즉시 저장한다.
	 *
	 * 중복은 막지 않는다 — 같은 키를 두 곳에 두는 것이 유용한 경우가 있고,
	 * 막으려면 "어느 쪽을 지울지" 를 물어야 해서 UI 공수가 늘어난다.
	 * 필요하면 UI 가 GetKeyMappings 로 미리 검사해 경고를 띄우면 된다.
	 *
	 * @return 실제로 바뀌었으면 true.
	 */
	UFUNCTION(BlueprintCallable, Category = "TD|Input")
	static bool SetKeyMapping(APlayerController* Player, FName MappingName, FKey NewKey);

	/** 전부 기본값으로. 되돌린 뒤 저장까지 한다. */
	UFUNCTION(BlueprintCallable, Category = "TD|Input")
	static bool ResetKeyMappingsToDefault(APlayerController* Player);
};
