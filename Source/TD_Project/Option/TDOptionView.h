#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Items/TDItemTypes.h"
#include "TDOptionView.generated.h"

class UTexture2D;

/**
 * 추가 옵션 창 목록에 표시할 장신구 하나입니다.
 *
 * 굴려진 옵션은 문장이 아니라 값(FTDItemOption) 그대로 보냅니다. 문구는
 * DT_OptionDefinition 에 있고 클라이언트도 그 테이블을 읽을 수 있으므로,
 * 완성된 FText 를 복제하면 같은 문장이 사람 수만큼 네트워크를 타게 됩니다.
 */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDOptionItemView
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

	/** 지금 붙어 있는 옵션의 등급. 한 번도 안 굴렸으면 아이템의 시작 등급입니다. */
	UPROPERTY(BlueprintReadOnly)
	FGameplayTag OptionRarity;

	/** 현재 굴려져 있는 줄들. 비어 있으면 아직 한 번도 굴리지 않은 것입니다. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FTDItemOption> Options;

	UPROPERTY(BlueprintReadOnly)
	int32 RerollCost = 0;

	/** 이번 굴림에서 등급이 오를 확률(0~1). 0이면 최고 등급입니다. */
	UPROPERTY(BlueprintReadOnly)
	float UpgradeChance = 0.f;

	/** 굴릴 수 있는 아이템인지. 풀이 지정되지 않은 장신구는 false 입니다. */
	UPROPERTY(BlueprintReadOnly)
	bool bCanReroll = false;
};

/** 서버에서 추가 옵션 창으로 전달하는 한 시점의 화면 정보입니다. */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDOptionWindowView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SessionId = 0;

	UPROPERTY(BlueprintReadOnly)
	int64 InventoryRevision = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Gold = 0;

	UPROPERTY(BlueprintReadOnly)
	TArray<FTDOptionItemView> Items;

	UPROPERTY(BlueprintReadOnly)
	FString Message;

	/** 0이면 일반 갱신, 양수이면 해당 재굴림 요청의 답변입니다. */
	UPROPERTY(BlueprintReadOnly)
	int32 ReplyRequestId = 0;

	/** 가방 내용이 변경됐을 때 기존 선택을 해제합니다. */
	UPROPERTY(BlueprintReadOnly)
	bool bResetSelection = true;
};
