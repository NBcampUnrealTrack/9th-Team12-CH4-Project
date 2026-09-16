#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDNameplateWidget.generated.h"

class UTextBlock;

/**
 * 캐릭터 머리 위에 뜨는 이름표. **클라이언트 표현이다.**
 *
 * 이름 값은 PlayerState 의 PlayerName 하나로 끝난다 — 캐릭터를 고를 때 서버가
 * SetPlayerName 을 부르고, 엔진이 그것을 전원에게 복제한다(TDPlayerState::SelectCharacter).
 * 별도의 복제 변수나 RPC 를 두지 않는다.
 *
 * ── 왜 매 틱 확인하는가 ──
 * 클라이언트에서는 캐릭터가 먼저 도착하고 PlayerState 가 나중에 붙는다. 위젯을 만들 때
 * 한 번만 읽으면 남의 캐릭터 이름표가 빈 칸으로 굳는다. 값이 **바뀐 순간에만** SetText 를
 * 부르므로 비용은 문자열 비교 한 번이다.
 */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDNameplateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 누구의 이름표인지 알려준다. 위젯은 자기가 누구 머리 위에 있는지 모른다 — 소유자가 알려준다. */
	UFUNCTION(BlueprintCallable, Category = "TD|Nameplate")
	void SetTargetPawn(APawn* InTargetPawn);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	// ── 거리 배율 ─────────────────────────────────────────
	// 화면 공간 위젯은 거리와 무관하게 같은 크기로 그려진다. 멀리 있는 사람의 이름이
	// 코앞의 이름과 똑같이 커 보이면 거리감이 사라진다. 몬스터 체력바와 같은 방식·같은 값이다.

	/** 카메라에서 이 거리(cm)일 때 배율 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|Nameplate|Distance", meta = (ClampMin = "1.0"))
	float ReferenceDistance = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|Nameplate|Distance", meta = (ClampMin = "0.01"))
	float MinDistanceScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|Nameplate|Distance", meta = (ClampMin = "0.01"))
	float MaxDistanceScale = 1.f;

private:
	void UpdateName();
	void UpdateDistanceScale();

	TWeakObjectPtr<APawn> TargetPawn;

	/** 마지막으로 넣은 이름. 매 틱 SetText 를 부르지 않으려고 기억한다. */
	FString DisplayedName;

	FVector2D BaseRenderScale = FVector2D(1.f, 1.f);
	bool bCapturedBaseRenderScale = false;
};
