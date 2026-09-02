#include "TDEnemyHealthBarWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "TDEnemyHealthBarWidget"

void UTDEnemyHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TargetHealthPercent = HpBar
		? FMath::Clamp(HpBar->GetPercent(), 0.0f, 1.0f)
		: 1.0f;

	DamageLagPercent = DamageLagProgressBar
		? FMath::Max(DamageLagProgressBar->GetPercent(), TargetHealthPercent)
		: TargetHealthPercent;

	if (HpBar)
	{
		HpBar->SetPercent(TargetHealthPercent);
	}

	if (DamageLagProgressBar)
	{
		DamageLagProgressBar->SetPercent(DamageLagPercent);
	}

	if (DamageFeedbackText)
	{
		DamageFeedbackText->SetRenderOpacity(0.0f);
		DamageFeedbackText->SetRenderTranslation(FVector2D::ZeroVector);
		DamageFeedbackText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTDEnemyHealthBarWidget::NativeDestruct()
{
	for (int32 Index = ActiveFeedbackEntries.Num() - 1; Index >= 0; --Index)
	{
		RemoveFeedbackAt(Index);
	}

	Super::NativeDestruct();
}

void UTDEnemyHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	for (int32 Index = ActiveFeedbackEntries.Num() - 1; Index >= 0; --Index)
	{
		FTDHealthFeedbackEntry& Entry = ActiveFeedbackEntries[Index];
		if (!Entry.TextBlock)
		{
			ActiveFeedbackEntries.RemoveAt(Index);
			continue;
		}

		Entry.TimeRemaining = FMath::Max(0.0f, Entry.TimeRemaining - InDeltaTime);
		const float Duration = FMath::Max(FeedbackDuration, KINDA_SMALL_NUMBER);
		const float Progress = 1.0f - Entry.TimeRemaining / Duration;
		Entry.TextBlock->SetRenderTranslation(
			FVector2D(0.0f, -FeedbackRiseDistance * FMath::Clamp(Progress, 0.0f, 1.0f)));

		const float FadeDuration = FMath::Clamp(FeedbackFadeDuration, KINDA_SMALL_NUMBER, Duration);
		const float Opacity = Entry.TimeRemaining < FadeDuration
			? Entry.TimeRemaining / FadeDuration
			: 1.0f;
		Entry.TextBlock->SetRenderOpacity(FMath::Clamp(Opacity, 0.0f, 1.0f));

		if (Entry.TimeRemaining <= 0.0f)
		{
			RemoveFeedbackAt(Index);
		}
	}

	if (DamageLagProgressBar)
	{
		if (DamageDelayRemaining > 0.0f)
		{
			DamageDelayRemaining = FMath::Max(0.0f, DamageDelayRemaining - InDeltaTime);
			return;
		}

		DamageLagPercent = FMath::FInterpTo(
			DamageLagPercent,
			TargetHealthPercent,
			InDeltaTime,
			DamageInterpSpeed);

		if (FMath::IsNearlyEqual(DamageLagPercent, TargetHealthPercent, 0.001f))
		{
			DamageLagPercent = TargetHealthPercent;
		}

		DamageLagProgressBar->SetPercent(DamageLagPercent);
	}
}

void UTDEnemyHealthBarWidget::SetHealth(float CurrentHealth, float MaxHealth, bool bIsCritical)
{
	const float SafeCurrentHealth = MaxHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(CurrentHealth, 0.0f, MaxHealth)
		: FMath::Max(0.0f, CurrentHealth);

	if (bHasHealthSample)
	{
		const float HealthDelta = SafeCurrentHealth - LastCurrentHealth;
		if (!FMath::IsNearlyZero(HealthDelta))
		{
			ShowHealthDelta(HealthDelta, bIsCritical);
		}
	}

	LastCurrentHealth = SafeCurrentHealth;
	bHasHealthSample = true;

	const float NewHealthPercent = MaxHealth > KINDA_SMALL_NUMBER
		? SafeCurrentHealth / MaxHealth
		: 0.0f;

	SetHealthPercent(NewHealthPercent);
}

void UTDEnemyHealthBarWidget::SetHealthPercent(float InHealthPercent)
{
	const float NewHealthPercent = FMath::Clamp(InHealthPercent, 0.0f, 1.0f);
	const bool bTookDamage = NewHealthPercent < TargetHealthPercent;

	TargetHealthPercent = NewHealthPercent;

	if (HpBar)
	{
		HpBar->SetPercent(TargetHealthPercent);
	}

	if (bTookDamage)
	{
		DamageDelayRemaining = DamageDelay;
		return;
	}

	DamageDelayRemaining = 0.0f;
	DamageLagPercent = TargetHealthPercent;

	if (DamageLagProgressBar)
	{
		DamageLagProgressBar->SetPercent(DamageLagPercent);
	}
}

void UTDEnemyHealthBarWidget::SetMonsterName(const FText& InMonsterName)
{
	if (MonsterNameText)
	{
		MonsterNameText->SetText(InMonsterName);
	}
}

void UTDEnemyHealthBarWidget::ShowHealthDelta(float HealthDelta, bool bIsCritical)
{
	if (!DamageFeedbackText || !DamageFeedbackLayer)
	{
		return;
	}

	const int32 EntryLimit = FMath::Max(1, MaxFeedbackEntries);
	while (ActiveFeedbackEntries.Num() >= EntryLimit)
	{
		RemoveFeedbackAt(0);
	}

	const bool bHealing = HealthDelta > 0.0f;
	const bool bCriticalDamage = bIsCritical && !bHealing;
	const int32 DisplayAmount = FMath::Max(1, FMath::RoundToInt(FMath::Abs(HealthDelta)));
	UTextBlock* FeedbackText = NewObject<UTextBlock>(this);
	if (!FeedbackText)
	{
		return;
	}

	const FText AmountText = FText::AsNumber(DisplayAmount);
	FeedbackText->SetText(bCriticalDamage
		? FText::Format(LOCTEXT("CriticalDamageFormat", "CRITICAL -{0}"), AmountText)
		: FText::Format(
			bHealing ? LOCTEXT("HealingFormat", "+{0}") : LOCTEXT("DamageFormat", "-{0}"),
			AmountText));

	FeedbackText->SetFont(DamageFeedbackText->GetFont());
	FeedbackText->SetJustification(ETextJustify::Center);
	FeedbackText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
	FeedbackText->SetShadowOffset(FVector2D(1.5f, 1.5f));
	FeedbackText->SetColorAndOpacity(FSlateColor(
		bHealing
			? FLinearColor(0.22f, 1.0f, 0.35f, 1.0f)
			: bCriticalDamage
				? FLinearColor(1.0f, 0.2f, 0.05f, 1.0f)
				: FLinearColor(1.0f, 0.82f, 0.28f, 1.0f)));
	FeedbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
	FeedbackText->SetRenderTranslation(FVector2D::ZeroVector);
	FeedbackText->SetRenderOpacity(1.0f);

	UCanvasPanelSlot* CanvasSlot = DamageFeedbackLayer->AddChildToCanvas(FeedbackText);
	if (!CanvasSlot)
	{
		FeedbackText->RemoveFromParent();
		return;
	}

	const float MinX = FMath::Min(FeedbackSpawnXRange.X, FeedbackSpawnXRange.Y);
	const float MaxX = FMath::Max(FeedbackSpawnXRange.X, FeedbackSpawnXRange.Y);
	const float MinY = FMath::Min(FeedbackSpawnYRange.X, FeedbackSpawnYRange.Y);
	const float MaxY = FMath::Max(FeedbackSpawnYRange.X, FeedbackSpawnYRange.Y);
	const FVector2D SpawnPosition(
		FMath::FRandRange(MinX, MaxX),
		FMath::FRandRange(MinY, MaxY));

	CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CanvasSlot->SetAutoSize(true);
	CanvasSlot->SetPosition(SpawnPosition);
	CanvasSlot->SetZOrder(ActiveFeedbackEntries.Num() + 1);

	FTDHealthFeedbackEntry& Entry = ActiveFeedbackEntries.AddDefaulted_GetRef();
	Entry.TextBlock = FeedbackText;
	Entry.TimeRemaining = FMath::Max(FeedbackDuration, 0.1f);
	Entry.SpawnPosition = SpawnPosition;
}

void UTDEnemyHealthBarWidget::RemoveFeedbackAt(int32 Index)
{
	if (!ActiveFeedbackEntries.IsValidIndex(Index))
	{
		return;
	}

	if (ActiveFeedbackEntries[Index].TextBlock)
	{
		ActiveFeedbackEntries[Index].TextBlock->RemoveFromParent();
	}

	ActiveFeedbackEntries.RemoveAt(Index);
}

#undef LOCTEXT_NAMESPACE
