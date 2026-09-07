#include "Chat/TDChatFilter.h"

namespace
{
	/**
	 * 검사에 쓸 글자인지. 나머지(공백·문장부호·이모지)는 정규화에서 버린다.
	 *
	 * 한글 범위를 직접 적는 이유는 IsAlnum 이 로케일에 따라 한글을 다르게 볼 수 있어서다.
	 * 호환 자모(ㄱ~ㅣ)를 남겨 두는 것은 의도적이다 — 초성을 자동 변환하지는 않지만,
	 * 금지어 목록에 "ㅅㅂ" 을 직접 넣으면 그건 잡히게 하려는 것이다.
	 */
	bool IsMeaningfulChar(TCHAR Ch)
	{
		if (FChar::IsAlnum(Ch))
		{
			return true;
		}

		const bool bHangulSyllable = Ch >= 0xAC00 && Ch <= 0xD7A3;	// 가 ~ 힣
		const bool bHangulJamo = Ch >= 0x3131 && Ch <= 0x318E;		// ㄱ ~ ㅣ

		return bHangulSyllable || bHangulJamo;
	}
}

namespace TDChatFilter
{
	FString Normalize(const FString& Message, TArray<int32>& OutSourceIndex)
	{
		OutSourceIndex.Reset();

		FString Result;
		Result.Reserve(Message.Len());
		OutSourceIndex.Reserve(Message.Len());

		for (int32 Index = 0; Index < Message.Len(); ++Index)
		{
			const TCHAR Ch = Message[Index];
			if (!IsMeaningfulChar(Ch))
			{
				continue;
			}

			Result.AppendChar(FChar::ToLower(Ch));

			// 남긴 글자가 원본 몇 번째에서 왔는지 함께 적어 둔다.
			// 이것이 없으면 정규화한 문자열에서 찾은 구간을 원본에서 가릴 수 없다.
			OutSourceIndex.Add(Index);
		}

		return Result;
	}

	FString Mask(const FString& Message, const TArray<FString>& BannedWords, bool& bOutMasked)
	{
		bOutMasked = false;

		if (Message.IsEmpty() || BannedWords.Num() == 0)
		{
			return Message;
		}

		TArray<int32> SourceIndex;
		const FString Normalized = Normalize(Message, SourceIndex);
		if (Normalized.IsEmpty())
		{
			return Message;
		}

		// 원본의 어느 글자를 가릴지 먼저 표시해 둔다. 찾자마자 문자열을 고치면
		// 뒤이어 찾을 금지어의 위치가 어긋난다.
		TArray<bool> bMaskChar;
		bMaskChar.Init(false, Message.Len());

		for (const FString& Word : BannedWords)
		{
			TArray<int32> UnusedIndex;
			const FString NormalizedWord = Normalize(Word, UnusedIndex);
			if (NormalizedWord.IsEmpty())
			{
				continue;
			}

			// 한 메시지에 같은 말이 여러 번 나올 수 있으므로 끝까지 훑는다.
			int32 SearchFrom = 0;
			for (;;)
			{
				const int32 Found = Normalized.Find(NormalizedWord,
					ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);

				if (Found == INDEX_NONE)
				{
					break;
				}

				// 정규화 좌표를 원본 좌표로 되돌린다. 사이에 끼어 있던 공백이나
				// 문장부호도 함께 가려야 "시 발" 이 "***" 로 보인다.
				const int32 StartInSource = SourceIndex[Found];
				const int32 EndInSource = SourceIndex[Found + NormalizedWord.Len() - 1];

				for (int32 Index = StartInSource; Index <= EndInSource; ++Index)
				{
					bMaskChar[Index] = true;
				}

				bOutMasked = true;
				SearchFrom = Found + NormalizedWord.Len();
			}
		}

		if (!bOutMasked)
		{
			return Message;
		}

		FString Result = Message;
		for (int32 Index = 0; Index < Result.Len(); ++Index)
		{
			if (bMaskChar[Index])
			{
				Result[Index] = TEXT('*');
			}
		}

		return Result;
	}
}
