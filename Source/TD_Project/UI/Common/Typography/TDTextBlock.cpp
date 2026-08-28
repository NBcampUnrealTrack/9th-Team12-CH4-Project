#include "TDTextBlock.h"

#include "UI/Settings/TDUISettings.h"

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
		return;
	}

	// Theme 또는 해당 Role이 비어 있으면 Common Text에 직접 지정한 Style을 유지한다.
	Super::SynchronizeProperties();
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
