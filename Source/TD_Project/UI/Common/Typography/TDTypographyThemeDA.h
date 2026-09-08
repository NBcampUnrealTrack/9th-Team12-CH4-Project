#pragma once

#include "CoreMinimal.h"
#include "CommonTextBlock.h"
#include "Engine/DataAsset.h"
#include "TDTypographyThemeDA.generated.h"

/** Tailwind의 text-* 또는 HTML의 H1/Body처럼 사용하는 의미 기반 텍스트 역할. */
UENUM(BlueprintType)
enum class ETDTextStyleRole : uint8
{
	H1 UMETA(DisplayName = "H1 - Main Title"),
	H2 UMETA(DisplayName = "H2 - Window Title"),
	H3 UMETA(DisplayName = "H3 - Section Title"),
	H4 UMETA(DisplayName = "H4 - Subsection Title"),
	H5 UMETA(DisplayName = "H5 - Item Title"),
	BodyLarge UMETA(DisplayName = "Body Large"),
	Body UMETA(DisplayName = "Body"),
	BodySmall UMETA(DisplayName = "Body Small"),
	Caption UMETA(DisplayName = "Caption"),
	Number UMETA(DisplayName = "Number")
};

/**
 * 프로젝트의 의미 기반 텍스트 역할과 Common Text Style 클래스를 연결한다.
 * 실제 폰트, 크기, 색상은 CTS_* 에셋에서 편집한다.
 */
UCLASS(BlueprintType)
class TD_PROJECT_API UTDTypographyThemeDA : public UDataAsset
{
	GENERATED_BODY()

public:
 /** 자유롭게 추가하는 이름별 스타일. 기존 역할 설정은 그대로 유지한다. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Custom Styles")
 TMap<FName, TSubclassOf<UCommonTextStyle>> CustomStyles;

	UFUNCTION(BlueprintPure, Category = "TD|Typography")
	TSubclassOf<UCommonTextStyle> GetStyle(ETDTextStyleRole Role) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Headings")
	TSubclassOf<UCommonTextStyle> H1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Headings")
	TSubclassOf<UCommonTextStyle> H2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Headings")
	TSubclassOf<UCommonTextStyle> H3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Headings")
	TSubclassOf<UCommonTextStyle> H4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Headings")
	TSubclassOf<UCommonTextStyle> H5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body")
	TSubclassOf<UCommonTextStyle> BodyLarge;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body")
	TSubclassOf<UCommonTextStyle> Body;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body")
	TSubclassOf<UCommonTextStyle> BodySmall;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Utility")
	TSubclassOf<UCommonTextStyle> Caption;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Utility")
	TSubclassOf<UCommonTextStyle> Number;
};
