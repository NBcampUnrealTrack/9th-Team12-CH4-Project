#include "TDGoogleSheetParser.h"

#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogTDSheet, Log, All);

namespace
{
	/**
	 * CSV 한 칸을 안전하게 감싼다.
	 *
	 * 값에 쉼표나 따옴표, 줄바꿈이 들어 있으면 따옴표로 묶고 내부 따옴표는 두 번 적는다.
	 * 아이템 설명처럼 사람이 쓴 문장에는 쉼표가 자주 들어가므로 반드시 필요하다.
	 */
	FString EscapeCsvCell(const FString& Value)
	{
		const bool bNeedsQuotes =
			Value.Contains(TEXT(",")) ||
			Value.Contains(TEXT("\"")) ||
			Value.Contains(TEXT("\n")) ||
			Value.Contains(TEXT("\r"));

		if (!bNeedsQuotes)
		{
			return Value;
		}

		FString Escaped = Value;
		Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""));

		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}
}

FString UTDGoogleSheetParser::BuildCsv() const
{
	// ParsedRows 는 헤더 문자열을 키로 쓰는 맵이다. 원본 헤더로 값을 찾아야 하므로
	// 공백을 걷어낸 이름은 CSV 에 쓸 때만 사용한다.
	const TArray<FString> RawHeaders = GetHeaders();

	TArray<FString> Lines;
	Lines.Reserve(GetRowCount() + 1);

	{
		TArray<FString> Cells;
		Cells.Reserve(RawHeaders.Num());

		for (const FString& RawHeader : RawHeaders)
		{
			Cells.Add(EscapeCsvCell(RawHeader.TrimStartAndEnd()));
		}

		Lines.Add(FString::Join(Cells, TEXT(",")));
	}

	for (int32 Index = 0; Index < GetRowCount(); ++Index)
	{
		TMap<FString, FString> Row;
		if (!GetRowAt(Index, Row))
		{
			continue;
		}

		TArray<FString> Cells;
		Cells.Reserve(RawHeaders.Num());

		for (const FString& RawHeader : RawHeaders)
		{
			const FString* Found = Row.Find(RawHeader);
			Cells.Add(EscapeCsvCell(Found ? Found->TrimStartAndEnd() : FString()));
		}

		// 첫 칸은 RowName 이다. 비어 있으면 시트 아래쪽의 빈 줄이므로 건너뛴다.
		if (Cells.Num() == 0 || Cells[0].IsEmpty())
		{
			continue;
		}

		Lines.Add(FString::Join(Cells, TEXT(",")));
	}

	return FString::Join(Lines, TEXT("\n"));
}

bool UTDGoogleSheetParser::OnParseComplete(FString& OutError)
{
	if (TargetTable == nullptr)
	{
		OutError = TEXT("Target Table 이 지정되지 않았다. Config 에서 대상 DataTable 을 골라야 한다.");
		return false;
	}

	if (TargetTable->GetRowStruct() == nullptr)
	{
		OutError = FString::Printf(
			TEXT("%s 에 Row Struct 가 없다. DataTable 을 만들 때 행 구조체를 지정해야 한다."),
			*TargetTable->GetName());
		return false;
	}

	if (GetRowCount() == 0)
	{
		OutError = TEXT("시트에서 읽은 행이 없다. Range 가 헤더 행부터 시작하는지 확인할 것.");
		return false;
	}

	const FString Csv = BuildCsv();

	// CreateTableFromCSVString 은 기존 행을 비우고 다시 채운다.
	// 시트에서 지운 행이 테이블에 남지 않는다는 뜻이라 시트를 원천으로 삼는 방식에 맞다.
	const int32 PreviousRowCount = TargetTable->GetRowMap().Num();

	TArray<FString> Problems = TargetTable->CreateTableFromCSVString(Csv);

	if (Problems.Num() > 0)
	{
		// 전부 보여주면 로그가 넘치므로 앞의 몇 개만 올린다. 원인은 대개 같다.
		const int32 ShowCount = FMath::Min(Problems.Num(), 5);

		FString Summary;
		for (int32 Index = 0; Index < ShowCount; ++Index)
		{
			Summary += FString::Printf(TEXT("\n  · %s"), *Problems[Index]);
		}

		if (Problems.Num() > ShowCount)
		{
			Summary += FString::Printf(TEXT("\n  · ... 외 %d건"), Problems.Num() - ShowCount);
		}

		OutError = FString::Printf(
			TEXT("%s 임포트 중 %d건의 문제가 있었다.%s\n"
				 "컬럼 이름이 구조체 프로퍼티와 정확히 같은지, 태그 문자열에 오타가 없는지 확인할 것."),
			*TargetTable->GetName(), Problems.Num(), *Summary);

		return false;
	}

	UE_LOG(LogTDSheet, Log,
		TEXT("%s — 시트에서 %d행을 읽었다. (이전 %d행)"),
		*TargetTable->GetName(), TargetTable->GetRowMap().Num(), PreviousRowCount);

	return true;
}
