#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TDChapterRow.generated.h"

class USoundBase;
class UTexture2D;

/**
 * DT_Chapter의 한 행.
 *
 * RowName이 ChapterId다.
 * 예: Chapter01, Chapter02
 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDChapterRow
	: public FTableRowBase
{
	GENERATED_BODY()

	/**
	 * 이 메인 퀘스트가 시작될 때 챕터 연출을 보여준다.
	 * 예: Main_01_01
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Chapter")
	FName StartQuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Chapter",
		meta = (ClampMin = "1"))
	int32 ChapterNumber = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Chapter")
	FText ChapterTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Chapter")
	TSoftObjectPtr<UTexture2D> BackgroundImage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Chapter")
	TSoftObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Chapter",
		meta = (ClampMin = "0.1"))
	float Duration = 3.0f;
};

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDChapterPresentationView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TD|Chapter")
	FName ChapterId;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Chapter")
	int32 ChapterNumber = 1;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Chapter")
	FText ChapterTitle;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Chapter")
	TSoftObjectPtr<UTexture2D> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Chapter")
	TSoftObjectPtr<USoundBase> Sound;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Chapter")
	float Duration = 3.0f;
};