#pragma once

#include "CoreMinimal.h"
#include "CommonTextBlock.h"
#include "TDTypographyThemeDA.h"
#include "TDTextBlock.generated.h"

class UTDTypographyThemeDA;

/**
 * WBP에서 CTS_* 에셋 대신 H1/H2/Body 같은 역할만 선택하는 공용 텍스트 위젯.
 */
UCLASS(BlueprintType, meta = (DisplayName = "TD Text"))
class TD_PROJECT_API UTDTextBlock : public UCommonTextBlock
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TD|Typography")
	void SetTextStyleRole(ETDTextStyleRole InStyleRole);

	UFUNCTION(BlueprintCallable, Category = "TD|Typography")
	void SetTypographyTheme(UTDTypographyThemeDA* InTheme);

	UFUNCTION(BlueprintPure, Category = "TD|Typography")
	ETDTextStyleRole GetTextStyleRole() const { return TextStyleRole; }

protected:
	virtual void SynchronizeProperties() override;

	/** HTML의 H1/Body 또는 Tailwind의 text-*처럼 선택하는 의미 기반 스타일. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Typography", meta = (ExposeOnSpawn = "true"))
	ETDTextStyleRole TextStyleRole = ETDTextStyleRole::Body;

	/** 지정한 경우 Project Settings의 기본 Theme 대신 이 Theme을 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Typography", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<UTDTypographyThemeDA> TypographyThemeOverride;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	UTDTypographyThemeDA* ResolveTypographyTheme();
	TSubclassOf<UCommonTextStyle> ResolveTextStyle();

	UPROPERTY(Transient)
	TObjectPtr<UTDTypographyThemeDA> LoadedDefaultTheme;

	/** SetStyle()이 다시 SynchronizeProperties()를 호출할 때 재귀하는 것을 막는다. */
	bool bApplyingTypographyStyle = false;
};
