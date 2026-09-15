#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TDSilhouetteComponent.generated.h"

class UCameraComponent;
class UMaterialInterface;

/**
 * 내 캐릭터가 벽·건물 뒤에 가려지면 가려진 부분에 윤곽을 칠한다. **클라이언트 표현이다.**
 *
 * ── 원리 ──
 * 스프라이트를 커스텀 뎁스에도 그리고, 카메라에 붙인 후처리 재질이 픽셀마다
 * "화면 깊이 < 캐릭터 깊이" 인 곳 — 캐릭터 앞에 무언가 있는 곳 — 만 색을 칠한다.
 * 벽의 재질은 건드리지 않는다.
 *
 * 가리는 물체에 원형 구멍을 내는 방식은 택하지 않았다. 레벨 팩 부모 재질 25개가량을
 * 모두 고쳐야 하고, 그 에셋은 레벨 담당의 것이다(2026-09-15).
 *
 * ── 내 캐릭터에서만 켠다 ──
 * 후처리는 커스텀 뎁스에 누가 그렸는지 구분하지 않는다. 남의 캐릭터까지 켜면
 * 그 윤곽도 내 화면에 칠해진다.
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDSilhouetteComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/**
	 * 로컬 플레이어가 조종하는 캐릭터일 때만 켠다. 여러 번 불려도 한 번만 붙는다.
	 *
	 * 캐릭터의 SetupPlayerInputComponent 가 부른다 — 로컬 컨트롤러가 빙의를 마친 뒤에만
	 * 불리는 자리라, 서버에 있는 남의 캐릭터에서는 불리지 않는다.
	 */
	void EnableForLocalPlayer();

private:
	/** 붙인 재질. 소프트 참조로 읽어 온 것이라 GC 되지 않게 잡아 둔다. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AppliedMaterial;

	/** 재질을 붙인 카메라. 같은 카메라에 두 번 붙이지 않으려고 기억한다. */
	TWeakObjectPtr<UCameraComponent> BoundCamera;
};
