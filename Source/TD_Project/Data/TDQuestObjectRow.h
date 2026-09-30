#pragma once

#include "CoreMinimal.h"
#include "Data/TDWorldCondition.h"
#include "Engine/DataTable.h"
#include "TDQuestObjectRow.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestObjectRow
	: public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|QuestObject")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|QuestObject")
	TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|QuestObject")
	FText InteractionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|QuestObject")
	FTDWorldCondition VisibilityCondition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|QuestObject")
	FName DefaultDialogueRow;

	/**
	 * true면 관련 퀘스트가 없을 때
	 * 해당 플레이어 화면에서 물건을 숨긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|QuestObject")
	bool bHideWhenNoRelevantQuest = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|QuestObject")
	bool bShowQuestMarker = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|QuestObject",
		meta = (ClampMin = "150.0"))
	float DialogueDistance = 350.0f;
};