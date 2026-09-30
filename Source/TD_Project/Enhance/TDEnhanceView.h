#pragma once

#include "CoreMinimal.h"
#include "Data/TDEnhanceRow.h"
#include "TDEnhanceView.generated.h"

class UTexture2D;

/** 강화창 목록에 표시할 장신구 하나입니다. */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDEnhanceItemView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	FName ItemId;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly)
	int32 EnhanceLevel = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bHasNextStep = false;

	UPROPERTY(BlueprintReadOnly)
	FTDEnhanceRow NextStep;
};

/** 서버에서 강화창으로 전달하는 한 시점의 화면 정보입니다. */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDEnhanceWindowView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SessionId = 0;

	UPROPERTY(BlueprintReadOnly)
	int64 InventoryRevision = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Gold = 0;

	UPROPERTY(BlueprintReadOnly)
	TArray<FTDEnhanceItemView> Items;

	UPROPERTY(BlueprintReadOnly)
	FString Message;

	/** 0이면 일반 갱신, 양수이면 해당 강화 요청의 답변입니다. */
	UPROPERTY(BlueprintReadOnly)
	int32 ReplyRequestId = 0;

	/** 가방 내용이 변경됐을 때 기존 선택을 해제합니다. */
	UPROPERTY(BlueprintReadOnly)
	bool bResetSelection = true;
};