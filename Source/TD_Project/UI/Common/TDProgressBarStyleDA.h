#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TDProgressBarStyleDA.generated.h"

UENUM(BlueprintType)
enum class ETDProgressBarAnimationMode : uint8
{
	Smooth,
	DamageTrail,
	LevelWrapped
};

/** 프로그레스바의 표시 방식과 색상. 실제 게임 수치는 포함하지 않는다. */
USTRUCT(BlueprintType)
struct FTDProgressBarStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
		ETDProgressBarAnimationMode Mode = ETDProgressBarAnimationMode::Smooth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation",
		meta = (ClampMin = "0", Units = "s"))
		float ChangeSeconds = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation",
		meta = (ClampMin = "0", Units = "s"))
		float RecoverySeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Trail",
		meta = (ClampMin = "0", Units = "s"))
		float TrailDelaySeconds = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Trail",
		meta = (ClampMin = "0", Units = "s"))
		float TrailSeconds = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Level",
		meta = (ClampMin = "0", Units = "s"))
		float LevelFillSeconds = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Level",
		meta = (ClampMin = "0", Units = "s"))
		float LevelHoldSeconds = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Flash",
		meta = (ClampMin = "0", Units = "s"))
		float FlashSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Flash",
		meta = (ClampMin = "0", ClampMax = "1"))
		float FlashOpacity = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color")
		FLinearColor FillTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color")
		FLinearColor TrailTint = FLinearColor(1.f, 0.55f, 0.08f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Color")
		FLinearColor FlashTint = FLinearColor::White;
};

/** 여러 HUD에서 공유하는 프로그레스바 스타일 에셋. 실행 중 진행률은 저장하지 않는다. */
UCLASS(BlueprintType)
class TD_PROJECT_API UTDProgressBarStyleDA : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress Bar")
		FTDProgressBarStyle Style;
};
