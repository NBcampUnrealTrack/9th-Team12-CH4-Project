#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDEnemyHealthBarWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UCanvasPanel;

USTRUCT()
struct FTDHealthFeedbackEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TextBlock;

	float TimeRemaining = 0.0f;
	FVector2D SpawnPosition = FVector2D::ZeroVector;
};

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDEnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Enemy Health Bar")
	void SetHealth(float CurrentHealth, float MaxHealth, bool bIsCritical = false);

	UFUNCTION(BlueprintCallable, Category = "Enemy Health Bar")
	void SetHealthPercent(float InHealthPercent);

	UFUNCTION(BlueprintCallable, Category = "Enemy Health Bar")
	void SetMonsterName(const FText& InMonsterName);

	UFUNCTION(BlueprintPure, Category = "Enemy Health Bar")
	float GetTargetHealthPercent() const { return TargetHealthPercent; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> HpBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> DamageLagProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MonsterNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> DamageFeedbackText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> DamageFeedbackLayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar|Animation", meta = (ClampMin = "0.0"))
	float DamageDelay = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar|Animation", meta = (ClampMin = "0.1"))
	float DamageInterpSpeed = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar|Feedback", meta = (ClampMin = "0.1"))
	float FeedbackDuration = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar|Feedback", meta = (ClampMin = "0.0"))
	float FeedbackRiseDistance = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar|Feedback", meta = (ClampMin = "0.05"))
	float FeedbackFadeDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar|Feedback")
	FVector2D FeedbackSpawnXRange = FVector2D(-70.0f, 70.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar|Feedback")
	FVector2D FeedbackSpawnYRange = FVector2D(-8.0f, 18.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar|Feedback", meta = (ClampMin = "1", UIMin = "1", UIMax = "30"))
	int32 MaxFeedbackEntries = 12;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy Health Bar")
	float TargetHealthPercent = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy Health Bar")
	float DamageLagPercent = 1.0f;

private:
	void ShowHealthDelta(float HealthDelta, bool bIsCritical);
	void RemoveFeedbackAt(int32 Index);

	float DamageDelayRemaining = 0.0f;
	float LastCurrentHealth = 0.0f;
	bool bHasHealthSample = false;

	UPROPERTY(Transient)
	TArray<FTDHealthFeedbackEntry> ActiveFeedbackEntries;
};
