#pragma once

#include "CoreMinimal.h"
#include "Data/TDWorldCondition.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "TDQuestTypes.generated.h"

class USoundBase;
class UTexture2D;

/**
 * 퀘스트 수락·완료·포기 요청의 결과.
 * 서버는 문장을 보내지 않고 이 결과만 보내며,
 * 실제 안내 문구는 UI에서 결정한다.
 */
UENUM(BlueprintType)
enum class ETDQuestActionResult : uint8
{
	Success UMETA(DisplayName = "성공"),
	InvalidDefinition UMETA(DisplayName = "데이터 설정 오류"),
	AlreadyAccepted UMETA(DisplayName = "이미 수락함"),
	AlreadyCompleted UMETA(DisplayName = "이미 완료함"),
	PrerequisiteNotMet UMETA(DisplayName = "선행 조건 불충족"),
	NotActive UMETA(DisplayName = "진행 중인 퀘스트가 아님"),
	NotReady UMETA(DisplayName = "아직 완료 조건 미충족"),
	InventoryFull UMETA(DisplayName = "인벤토리 부족"),
	RequiredItemMissing UMETA(DisplayName = "제출 아이템 부족"),
	ActiveSubQuestLimit UMETA(DisplayName = "서브 퀘스트 최대 개수"),
	MainQuestAlreadyActive UMETA(DisplayName = "이미 메인 퀘스트 진행 중"),
	CannotAbandonMain UMETA(DisplayName = "메인 퀘스트 포기 불가"),
	WrongAcceptTarget UMETA(DisplayName = "잘못된 수락 대상"),
	WrongTurnInTarget UMETA(DisplayName = "잘못된 완료 대상"),
	DailyAlreadyCompleted UMETA(DisplayName = "오늘 이미 완료함")
};

/** 퀘스트 목표의 종류 */
UENUM(BlueprintType)
enum class ETDQuestObjectiveType : uint8
{
	/**
	 * 기존 EventTag 방식.
	 * 현재 만들어 둔 대화형 퀘스트와의 호환을 위해 첫 번째 값으로 유지한다.
	 */
	GameplayEvent UMETA(DisplayName = "일반 이벤트"),

	KillMonster UMETA(DisplayName = "몬스터 처치"),
	OwnItem UMETA(DisplayName = "아이템 보유"),
	EnterZone UMETA(DisplayName = "지역 진입"),
	OpenChest UMETA(DisplayName = "상자 열기")
};

/** 반복 규칙 */
UENUM(BlueprintType)
enum class ETDQuestRepeatType : uint8
{
	None UMETA(DisplayName = "반복 불가"),

	/**
	 * 이름은 24시간이지만 실제 판정은 24시간 타이머가 아니라
	 * 한국 시간 00:00 날짜 변경 기준이다.
	 */
	Cooldown24Hours UMETA(DisplayName = "한국 시간 자정 초기화")
};

/** 퀘스트를 주거나 완료받는 대상의 종류 */
UENUM(BlueprintType)
enum class ETDQuestTargetType : uint8
{
	None UMETA(DisplayName = "대상 없음"),
	NPC UMETA(DisplayName = "NPC"),
	QuestObject UMETA(DisplayName = "퀘스트 물건")
};

/** NPC 머리 위에 표시할 기호 */
UENUM(BlueprintType)
enum class ETDQuestMarkerType : uint8
{
	None UMETA(DisplayName = "표시 없음"),
	Available UMETA(DisplayName = "수락 가능 !"),
	TurnIn UMETA(DisplayName = "완료 가능 ?")
};

/** 퀘스트 목표 한 개 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestObjectiveDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	ETDQuestObjectiveType ObjectiveType =
		ETDQuestObjectiveType::GameplayEvent;

	/** HUD에 표시할 목표 설명 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	FText Description;

	/**
	 * KillMonster:
	 *     DT_MonsterDefinition RowName
	 *
	 * OwnItem:
	 *     DT_ItemDefinition RowName
	 *
	 * OpenChest:
	 *     DT_TreasureChest RowName
	 *
	 * GameplayEvent:
	 *     해당 목표의 ! 마커를 표시할
	 *     NPCId 또는 QuestObjectId
	 *
	 * OpenChest에서 비어 있으면
	 * 아무 상자 열기 목표로 처리한다.
	 *
	 * GameplayEvent에서 비어 있으면
	 * 기존 AcceptTarget 또는 TurnInTarget을 사용한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|Quest")
	FName TargetId;

	/** EnterZone 목표에서 사용하는 정확한 Zone 태그 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	FGameplayTag TargetZone;

	/** GameplayEvent 목표에서 사용하는 이벤트 태그 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest",
		meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	/**
	 * OwnItem 목표에서만 사용한다.
	 *
	 * true면 완료 보고 순간 아이템을 실제로 제거한다.
	 * false면 아이템을 가지고 있기만 하면 되고 제출하지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	bool bConsumeOnTurnIn = false;
};

/** 아이템 보상 하나 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestItemReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest",
		meta = (ClampMin = "1"))
	int32 Count = 1;
};

/** 퀘스트 수락에 필요한 NPC 호감도 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestAffectionRequirement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	FName NPCId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest",
		meta = (ClampMin = "0"))
	int32 RequiredPoints = 0;
};

/** 퀘스트 완료 시 지급할 NPC 호감도 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestAffectionReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	FName NPCId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest",
		meta = (ClampMin = "0"))
	int32 Amount = 0;
};

/**
 * 진행 중인 퀘스트에서 들을 수 있는 반복 안내 대사.
 *
 * 예:
 * N1에게 반지를 물어보는 단계에서도
 * 김희진에게 돌아가면 반지 관련 안내를 들을 수 있다.
 *
 * 이 데이터 자체는 퀘스트 진행도나 NPC 마커를 변경하지 않는다.
 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestProgressDialogueRule
{
	GENERATED_BODY()

	/** 안내 대사를 하는 NPC의 DT_NPC RowName */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Dialogue")
	FName NPCId;

	/** 안내 대화가 시작되는 DT_Dialogue RowName */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Dialogue")
	FName StartDialogueRow;
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

	/**
	 * 캐릭터가 월드에 처음 진입했을 때 자동으로 지급할 첫 메인 퀘스트.
	 * DT_Quest 전체에서 정확히 하나만 체크한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Main")
	bool bInitialMainQuest = false;

	/** 메인 완료 후 자동으로 시작할 다음 DT_Quest RowName */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Main")
	FName NextQuestId;

	/**
	 * 업데이트 대기용 메인 퀘스트.
	 *
	 * true인 동안 목표가 없어도 Ready가 되지 않고 Active로 남는다.
	 * 업데이트할 때 false로 바꾸고 bAutoCompleteWithoutTurnIn을 true로 바꾸면
	 * 다음 접속에서 자동으로 완료한 뒤 NextQuestId로 이동한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Main")
	bool bWaitForFutureContent = false;

	/**
	 * NPC나 물건 보고 없이 자동 완료할 특수 퀘스트.
	 * 일반 퀘스트에서는 반드시 false로 둔다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Main")
	bool bAutoCompleteWithoutTurnIn = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	ETDQuestRepeatType RepeatType =
		ETDQuestRepeatType::None;

	/** 모두 완료한 기록이 있어야 수락 가능 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Condition")
	TArray<FName> PrerequisiteQuestIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Condition",
		meta = (ClampMin = "0"))
	int32 MinimumLevel = 0;

	/**
	 * 비어 있으면 모든 직업.
	 * 값이 있으면 해당 ClassId 중 하나일 때만 보인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Condition")
	TArray<FName> AllowedClassIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Condition")
	TArray<FTDQuestAffectionRequirement> AffectionRequirements;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Condition")
	FTDWorldCondition AcceptCondition;

	/** 서브/일일 퀘스트를 주는 NPC 또는 물건 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Interaction")
	ETDQuestTargetType AcceptTargetType =
		ETDQuestTargetType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Interaction")
	FName AcceptTargetId;

	/** 완료 보고를 받는 NPC 또는 물건 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Interaction")
	ETDQuestTargetType TurnInTargetType =
		ETDQuestTargetType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Interaction")
	FName TurnInTargetId;

	/** DT_Dialogue RowName */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Dialogue")
	FName OfferDialogueRow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Dialogue")
	FName InProgressDialogueRow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Dialogue")
	FName TurnInDialogueRow;

	/**
	 * 이 퀘스트가 진행 중일 때 NPC별로 들을 수 있는 반복 안내.
	 *
	 * NPCId:
	 *     안내 대사를 하는 NPC의 ID.
	 *
	 * StartDialogueRow:
	 *     안내 대화의 첫 번째 DT_Dialogue RowName.
	 *
	 * 여러 NPC를 등록할 수 있으며 배열 순서대로 검사한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Dialogue")
	TArray<FTDQuestProgressDialogueRule> ProgressDialogueRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	TArray<FTDQuestObjectiveDefinition> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Reward")
	TArray<FTDQuestItemReward> ItemRewards;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Reward",
		meta = (ClampMin = "0"))
	int32 ExpReward = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Reward",
		meta = (ClampMin = "0"))
	int32 GoldReward = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Reward")
	TArray<FTDQuestAffectionReward> AffectionRewards;

	/** 기존 NPC 대화 조건과의 호환을 위한 상태 태그 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Tags")
	FGameplayTag AcceptedTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Tags")
	FGameplayTag ReadyTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest|Tags")
	FGameplayTag CompletedTag;
};

/** 저장되고 소유 플레이어에게 복제되는 퀘스트 진행 상태 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestRuntimeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	FGameplayTag StateTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	TArray<int32> ObjectiveProgress;

	/** 서브 퀘스트를 HUD에서 수락 순서대로 정렬할 때 사용 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	int64 AcceptSequence = 0;

	/**
	 * 일일 퀘스트 완료 날짜.
	 * 예: 20260906
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Quest")
	int32 CompletedKstDayKey = 0;
};

/** 캐릭터별 NPC 호감도 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDAffectionRuntimeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Affection")
	FName NPCId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Affection")
	int32 Points = 0;

	/** 이 NPC에게 마지막으로 선물한 한국 날짜 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Affection")
	int32 LastGiftKstDayKey = 0;
};

/** HUD 목표 한 줄 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestObjectiveView
{
	GENERATED_BODY()

	/** HUD에 표시할 목표 설명 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	FText Description;

	/** 현재 진행 수치 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	int32 CurrentCount = 0;

	/** 목표 수치 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	int32 RequiredCount = 1;

	/** 목표를 모두 달성했는지 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	bool bCompleted = false;

	/**
	 * true일 때만 (현재 수치/목표 수치)를 표시한다.
	 *
	 * 몬스터 처치와 아이템 보유 목표만 true이며,
	 * NPC 대화, 지역 진입, 상자 열기 목표는 false다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	bool bShowNumericProgress = false;
};

/** HUD와 퀘스트창에 전달하는 퀘스트 한 개 */
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

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	TArray<FTDQuestItemReward> ItemRewards;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	int32 ExpReward = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	int32 GoldReward = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	bool bDailyQuest = false;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	int64 AcceptSequence = 0;
};

/** NPC 또는 물건 머리 위 표시용 정보 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDQuestMarkerView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	ETDQuestMarkerType MarkerType =
		ETDQuestMarkerType::None;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	FName QuestId;

	/** 메인/서브 색상을 정할 때 사용 */
	UPROPERTY(BlueprintReadOnly, Category = "TD|Quest")
	FGameplayTag QuestTypeTag;
};