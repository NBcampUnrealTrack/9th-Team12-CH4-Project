#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "TDDialogueRow.generated.h"

class UTexture2D;

/**
 * 대화 한 줄을 넘길 때 서버가 수행할 작업.
 *
 * ActionType:
 * Dialogue.Action.AcceptQuest
 * Dialogue.Action.TurnInQuest
 * Dialogue.Action.ReportQuestEvent
 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDDialogueAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Dialogue")
	FGameplayTag ActionType;

	/** AcceptQuest 또는 TurnInQuest에서 사용하는 DT_Quest RowName */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Dialogue")
	FName QuestId;

	/** ReportQuestEvent에서 사용하는 이벤트 태그 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Dialogue")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Dialogue",
		meta = (ClampMin = "1"))
	int32 EventAmount = 1;
};

/** DT_Dialogue의 한 행 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDDialogueRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 체크하면 플레이어가 말하는 줄, 끄면 NPC가 말하는 줄 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Dialogue")
	bool bPlayerSpeaker = false;

	/**
	 * 비워 두면 C++가 플레이어 이름 또는 NPC 이름을 자동으로 사용한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Dialogue")
	FText SpeakerNameOverride;

	/**
	 * 특별한 표정 등 기본 초상화를 덮어쓸 때만 지정한다.
	 * 비워 두면 NPC 기본 초상화를 사용한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Dialogue")
	TSoftObjectPtr<UTexture2D> PortraitOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Dialogue",
		meta = (MultiLine = "true"))
	FText DialogueText;

	/** 다음 DT_Dialogue RowName. None이면 이 줄 이후 대화 종료 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Dialogue")
	FName NextRow;

	/** 이 줄에서 다음 버튼을 눌렀을 때 서버가 수행할 작업 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Dialogue")
	FTDDialogueAction OnAdvanceAction;

	/** 다음, 수락, 완료, 닫기 등의 버튼 문구 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Dialogue")
	FText ContinueButtonText;
};

/** 서버가 해당 플레이어의 대화 UI에 보내는 한 줄 */
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
};