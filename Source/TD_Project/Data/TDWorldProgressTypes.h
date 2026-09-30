#pragma once

#include "CoreMinimal.h"
#include "TDWorldProgressTypes.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class ETDChestResetType : uint8
{
	OneTime UMETA(DisplayName = "캐릭터당 1회"),
	DailyKST UMETA(DisplayName = "한국 시간 자정 초기화")
};

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDChestClaimRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Treasure")
	FName ChestId;

	/**
	 * OneTime 상자는 0.
	 * DailyKST 상자는 마지막 획득 날짜(YYYYMMDD).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Treasure")
	int32 LastClaimKstDayKey = 0;
};

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDNpcGiftRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Affection")
	FName NPCId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Affection")
	int32 LastGiftKstDayKey = 0;
};

UENUM(BlueprintType)
enum class ETDGiftActionResult : uint8
{
	Success UMETA(DisplayName = "성공"),
	InvalidNPC UMETA(DisplayName = "NPC 오류"),
	InvalidItem UMETA(DisplayName = "아이템 오류"),
	QuestItemNotAllowed UMETA(DisplayName = "퀘스트 아이템 선물 불가"),
	AlreadyGiftedToday UMETA(DisplayName = "오늘 이미 선물함"),
	ConsumeFailed UMETA(DisplayName = "아이템 제거 실패")
};

UENUM(BlueprintType)
enum class ETDGiftItemLocation : uint8
{
	Inventory UMETA(DisplayName = "인벤토리"),
	Equipped UMETA(DisplayName = "장착 중")
};

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDGiftItemView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TD|Affection")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Affection")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Affection")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Affection")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Affection")
	int32 Count = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Affection")
	ETDGiftItemLocation Location =
		ETDGiftItemLocation::Inventory;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Affection")
	bool bCanGift = false;
};

USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDGiftResultView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TD|Affection")
	FName NPCId;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Affection")
	ETDGiftActionResult Result =
		ETDGiftActionResult::InvalidNPC;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Affection")
	FText NPCResponse;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Affection")
	int32 AffectionGained = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TD|Affection")
	int32 TotalAffection = 0;
};