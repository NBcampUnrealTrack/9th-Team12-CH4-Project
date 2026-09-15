#pragma once

#include "CoreMinimal.h"
#include "TD2DFlipbookSet.generated.h"

class UPaperFlipbook;

/** 2D 보스 계열 BP에서 직접 지정하는 4방향 PaperFlipbook 묶음. */
USTRUCT(BlueprintType)
struct FTD2DDirectionalFlipbooks
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flipbook")
	TObjectPtr<UPaperFlipbook> North;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flipbook")
	TObjectPtr<UPaperFlipbook> East;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flipbook")
	TObjectPtr<UPaperFlipbook> South;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flipbook")
	TObjectPtr<UPaperFlipbook> West;
};
