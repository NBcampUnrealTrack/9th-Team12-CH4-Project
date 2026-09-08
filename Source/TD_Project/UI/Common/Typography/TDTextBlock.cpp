#include "TDTextBlock.h"

#include "UI/Settings/TDUISettings.h"

void UTDTextBlock::SetCustomStyleName(FName InStyleName)
{
    if (CustomStyleName != InStyleName)
    {
        CustomStyleName = InStyleName;
        SynchronizeProperties();
    }
}

TArray<FString> UTDTextBlock::GetCustomStyleNames()
{
    TArray<FString> Names;
    if (const UTDTypographyThemeDA* Theme = ResolveTypographyTheme())
    {
        for (const auto& Entry : Theme->CustomStyles)
        {
            if (!Entry.Key.IsNone())
            {
                Names.Add(Entry.Key.ToString());
            }
        }
    }
    Names.Sort();
    Names.Insert(TEXT("None"), 0);
    return Names;
}

void UTDTextBlock::SetTextStyleRole(ETDTextStyleRole InStyleRole)
{
	if (TextStyleRole == InStyleRole)
	{
		return;
	}

	TextStyleRole = InStyleRole;
	SynchronizeProperties();
}

void UTDTextBlock::SetTypographyTheme(UTDTypographyThemeDA* InTheme)
{
	if (TypographyThemeOverride == InTheme)
	{
		return;
	}

	TypographyThemeOverride = InTheme;
	SynchronizeProperties();
}

void UTDTextBlock::SynchronizeProperties()
{
	// UCommonTextBlock::SetStyle()이 이 함수를 다시 부르므로 중첩 호출에서는
	// 부모 구현만 실행해 최종 Slate 속성을 갱신한다.
	if (bApplyingTypographyStyle)
	{
		Super::SynchronizeProperties();
		return;
	}

	if (const TSubclassOf<UCommonTextStyle> ResolvedStyle = ResolveTextStyle())
	{
		TGuardValue<bool> ApplyingGuard(bApplyingTypographyStyle, true);
		SetStyle(ResolvedStyle);
	}
	else
	{
		// Theme 또는 해당 Role이 비어 있으면 Common Text에 직접 지정한 Style을 유지한다.
		Super::SynchronizeProperties();
	}

	// 공통 Style 적용이 끝난 뒤 WBP 인스턴스의 예외값을 마지막에 덮어쓴다.
	ApplyInstanceOverrides();
}

void UTDTextBlock::ApplyInstanceOverrides()
{
	if (bOverrideFontSize)
	{
		FSlateFontInfo FontInfo = GetFont();
		FontInfo.Size = FontSizeOverride;
		SetFont(FontInfo);
	}

	if (bOverrideColor)
	{
		SetColorAndOpacity(FSlateColor(ColorOverride));
	}
}

UTDTypographyThemeDA* UTDTextBlock::ResolveTypographyTheme()
{
	if (TypographyThemeOverride)
	{
		return TypographyThemeOverride;
	}

	const UTDUISettings* UISettings = GetDefault<UTDUISettings>();
	if (!UISettings || UISettings->DefaultTypographyTheme.IsNull())
	{
		LoadedDefaultTheme = nullptr;
		return nullptr;
	}

	LoadedDefaultTheme = UISettings->DefaultTypographyTheme.LoadSynchronous();
	return LoadedDefaultTheme;
}

TSubclassOf<UCommonTextStyle> UTDTextBlock::ResolveTextStyle()
{
	if (const UTDTypographyThemeDA* Theme = ResolveTypographyTheme())
	{
		if (!CustomStyleName.IsNone())
		{
			const TSubclassOf<UCommonTextStyle>* CustomStyle = Theme->CustomStyles.Find(CustomStyleName);
			if (CustomStyle && *CustomStyle)
			{
				return *CustomStyle;
			}
		}
		return Theme->GetStyle(TextStyleRole);
	}

	return nullptr;
}

#if WITH_EDITOR
const FText UTDTextBlock::GetPaletteCategory()
{
	return NSLOCTEXT("TDUI", "PaletteCategory", "TD UI");
}
#endif
