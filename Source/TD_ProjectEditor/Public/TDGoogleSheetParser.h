#pragma once

#include "CoreMinimal.h"
#include "GoogleSheetParserBase.h"
#include "TDGoogleSheetParser.generated.h"

class UDataTable;

/**
 * 모든 DataTable 에 쓰는 범용 시트 파서.
 *
 * 테이블마다 파서를 만들지 않는 이유는 언리얼이 이미 CSV 임포터를 갖고 있기 때문이다.
 * 시트에서 받은 셀을 CSV 문자열로 다시 조립해 UDataTable::CreateTableFromCSVString 에
 * 넘기면, 컬럼 이름과 구조체 필드를 리플렉션으로 알아서 맞춰준다.
 *
 *   · FGameplayTag, FText, enum, TSoftObjectPtr 변환을 직접 짤 필요가 없다
 *   · 시트에 컬럼이 늘어도 이 코드는 그대로다
 *   · 테이블이 몇 개가 되든 파서는 이거 하나면 된다
 *
 * 대신 시트가 규칙을 지켜야 한다.
 *   · 첫 행은 헤더이고, 첫 컬럼 이름은 Name 이어야 한다(RowName 이 된다)
 *   · 컬럼 이름은 구조체의 프로퍼티 이름과 정확히 같아야 한다
 *   · 태그는 Stat.Offense.Damage 처럼 문자열로 적는다
 *
 * 앞뒤 공백은 이쪽에서 걷어낸다. 사람이 시트에 입력하는 이상 계속 섞여 들어오고,
 * 그대로 두면 FName 이 "Ring " 이 되어 아이템 매칭이 조용히 실패한다.
 */
UCLASS(EditInlineNew, DisplayName = "TD DataTable Parser")
class TD_PROJECTEDITOR_API UTDGoogleSheetParser : public UGoogleSheetParserBase
{
	GENERATED_BODY()

public:
	/**
	 * 시트 내용을 받을 DataTable. RowStruct 가 시트 컬럼과 맞아야 한다.
	 *
	 * 로드하면 **기존 행이 전부 교체된다.** 시트에서 지운 행은 테이블에서도 사라지므로
	 * 시트가 항상 완전한 목록이어야 한다. 병합은 지원하지 않는다 —
	 * 어느 쪽이 최신인지 알 수 없는 상태를 만들지 않기 위해서다.
	 */
	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<UDataTable> TargetTable;

protected:
	virtual bool OnParseComplete(FString& OutError) override;

private:
	/** 시트에서 받은 셀들을 CSV 한 덩어리로 조립한다. */
	FString BuildCsv() const;
};
