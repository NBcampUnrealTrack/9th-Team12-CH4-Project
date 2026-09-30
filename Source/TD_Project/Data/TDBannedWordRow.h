#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TDBannedWordRow.generated.h"

/**
 * DT_BannedWord 의 행. 걸러낼 말 하나다.
 *
 * RowName 은 편집용 별칭이고 실제로 쓰이는 값은 Word 다. 같은 말의 변형을
 * 여러 줄로 넣을 때 RowName 을 "Word01", "Word02" 처럼 두면 편하다.
 *
 * 검사는 정규화한 문자열에서 한다(TDChatFilter) — 공백·특수문자·반복 문자를
 * 걷어낸 뒤 비교하므로, 여기에는 **띄어쓰기 없는 기본형 하나만** 넣으면 된다.
 * "시 발", "시!발" 같은 변형을 일일이 넣을 필요가 없다.
 */
USTRUCT(BlueprintType)
struct FTDBannedWordRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 걸러낼 말. 비어 있으면 그 행은 무시한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chat")
	FString Word;
};
