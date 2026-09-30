#pragma once

#include "CoreMinimal.h"
#include "Data/TDQuestTypes.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "TDDialogueRow.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDDialogueAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Dialogue")
	FGameplayTag ActionType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Dialogue")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Dialogue")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Dialogue",
		meta = (ClampMin = "1"))
	int32 EventAmount = 1;
};

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDDialogueRow
	: public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Dialogue")
	bool bPlayerSpeaker = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Dialogue")
	FText SpeakerNameOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Dialogue")
	TSoftObjectPtr<UTexture2D> PortraitOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Dialogue",
		meta = (MultiLine = "true"))
	FText DialogueText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Dialogue")
	FName NextRow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Dialogue")
	FTDDialogueAction OnAdvanceAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Dialogue")
	FText ContinueButtonText;
};

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDDialogueLineView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TD|Dialogue")
	bool bPlayerSpeaker = false;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Dialogue")
	FText SpeakerName;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Dialogue")
	TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Dialogue")
	FText DialogueText;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Dialogue")
	FText ContinueButtonText;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Dialogue")
	bool bShowAcceptDecline = false;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Dialogue")
	bool bShowGiftButton = false;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Dialogue")
	bool bGiftAvailableToday = false;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	FName OfferedQuestId;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	FText OfferedQuestName;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	FText OfferedQuestDescription;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	TArray<FTDQuestObjectiveDefinition>
		OfferedQuestObjectives;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	TArray<FTDQuestItemReward>
		OfferedQuestItemRewards;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	int32 OfferedQuestExpReward = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	int32 OfferedQuestGoldReward = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	TArray<FTDQuestAffectionReward>
		OfferedQuestAffectionRewards;
};