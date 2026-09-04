#pragma once

#include "CoreMinimal.h"
#include "Data/TDWorldCondition.h"
#include "Engine/DataTable.h"
#include "TDNPCRow.generated.h"

class UTexture2D;

/**
 * NPC가 어떤 대화부터 시작할지 고르는 규칙.
 * 배열의 위쪽 규칙부터 검사하며 첫 번째로 맞는 규칙을 사용한다.
 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDNPCDialogueRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|NPC")
	FTDWorldCondition Condition;

	/** DT_Dialogue의 RowName */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|NPC")
	FName StartDialogueRow;
};

/** DT_NPC의 한 행 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDNPCRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|NPC")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|NPC")
	TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|NPC")
	FText InteractionText;

	/**
	 * NPC 자체가 보일 조건.
	 * 비어 있으면 항상 보인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|NPC")
	FTDWorldCondition VisibilityCondition;

	/**
	 * 가장 구체적인 조건부터 배치해야 한다.
	 *
	 * 권장 순서:
	 * Completed
	 * Ready
	 * Accepted
	 * 선행 조건 충족
	 * 조건 없는 일반 대화
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|NPC")
	TArray<FTDNPCDialogueRule> DialogueRules;
};