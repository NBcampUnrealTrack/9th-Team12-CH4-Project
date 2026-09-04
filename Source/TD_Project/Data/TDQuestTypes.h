#pragma once

#include "CoreMinimal.h"
#include "Data/TDWorldCondition.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "TDQuestTypes.generated.h"

/**
 * 퀘스트 수락·완료 요청 결과.
 * 완성된 문장이 아니라 결과만 보내고 실제 안내 문구는 UI가 결정한다.
 */
UENUM(BlueprintType)
enum class ETDQuestActionResult : uint8
{
	Success UMETA(DisplayName = "성공"),
	InvalidDefinition UMETA(DisplayName = "데이터 설정 오류"),
	AlreadyAccepted UMETA(DisplayName = "이미 수락함"),
	AlreadyCompleted UMETA(DisplayName = "이미 완료함"),
	PrerequisiteNotMet UMETA(DisplayName = "선행 조건 불충족"),
	NotActive UMETA(DisplayName = "수락하지 않은 퀘스트"),
	NotReady UMETA(DisplayName = "아직 완료 조건 미충족"),
	InventoryFull UMETA(DisplayName = "인벤토리 부족")
};

/** 퀘스트 목표 하나의 정의 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestObjectiveDefinition
{
	GENERATED_BODY()

	/** 퀘스트 UI에 표시할 목표 설명 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	FText Description;

	/**
	 * 이 목표를 진행시키는 이벤트 태그.
	 * 예: Quest.Event.Talk.HanSuhyun
	 *     Quest.Event.Kill.Slime
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest",
		meta = (ClampMin = "1"))
	int32 RequiredCount = 1;
};

/** 퀘스트 아이템 보상 하나 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestItemReward
{
	GENERATED_BODY()

	/** DT_ItemDefinition의 RowName */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest",
		meta = (ClampMin = "1"))
	int32 Count = 1;
};

/** DT_Quest의 한 행 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Quest.Type.Main 또는 Quest.Type.Sub */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	FGameplayTag QuestTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest",
		meta = (MultiLine = "true"))
	FText Description;

	/** 퀘스트를 수락하기 위해 필요한 조건 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	FTDWorldCondition AcceptCondition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	TArray<FTDQuestObjectiveDefinition> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Reward")
	TArray<FTDQuestItemReward> ItemRewards;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Reward",
		meta = (ClampMin = "0"))
	int32 ExpReward = 0;

	/**
	 * NPC 대화 분기·NPC 표시·상자 조건에서 사용할 퀘스트별 태그.
	 *
	 * 예:
	 * Quest.Main.Prologue01.Accepted
	 * Quest.Main.Prologue01.Ready
	 * Quest.Main.Prologue01.Completed
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Tags")
	FGameplayTag AcceptedTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Tags")
	FGameplayTag ReadyTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Tags")
	FGameplayTag CompletedTag;
};

/** 저장·복제되는 퀘스트 한 개의 실제 진행 상태 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestRuntimeData
{
	GENERATED_BODY()

	/** DT_Quest의 RowName */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	FName QuestId;

	/**
	 * Quest.State.Active
	 * Quest.State.ReadyToTurnIn
	 * Quest.State.Completed
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	FGameplayTag StateTag;

	/** Objectives 배열과 같은 순서 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	TArray<int32> ObjectiveProgress;
};

/** UI에 전달할 목표 표시 정보 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestObjectiveView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	int32 CurrentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	int32 RequiredCount = 1;
};

/** 퀘스트창과 퀘스트 추적 UI가 사용하는 표시용 정보 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	FName QuestId;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	FGameplayTag QuestTypeTag;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	FGameplayTag StateTag;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	TArray<FTDQuestObjectiveView> Objectives;
};