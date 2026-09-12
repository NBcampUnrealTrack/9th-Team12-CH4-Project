#pragma once

#include "CoreMinimal.h"
#include "Data/TDWorldCondition.h"
#include "Engine/DataTable.h"
#include "TDNPCRow.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDNPCDialogueRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	FTDWorldCondition Condition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	FName StartDialogueRow;
};

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDNPCGiftPreference
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC|Gift")
	FName ItemId;

	/**
	 * 0도 유효하다.
	 * 아이템과 일일 기회는 소비되고 호감도만 0 오른다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC|Gift",
		meta = (ClampMin = "0"))
	int32 AffectionGain = 0;
};

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDNPCRow
	: public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	FText InteractionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	FTDWorldCondition VisibilityCondition;

	/**
	 * 퀘스트 대화가 하나도 없을 때 사용할 일반 대화.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	FName DefaultDialogueRow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	TArray<FTDNPCDialogueRule> DialogueRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	bool bShowQuestMarker = true;
	
	/** 정상적인 대화 종료 후 강화창을 여는 NPC인지 여부입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|NPC|Enhance")
	bool bOpenEnhanceAfterDialogue = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC|Gift")
	bool bAcceptsGifts = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC|Gift",
		meta = (MultiLine = "true"))
	FText GiftThankYouText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC|Gift")
	TArray<FTDNPCGiftPreference> GiftPreferences;
};