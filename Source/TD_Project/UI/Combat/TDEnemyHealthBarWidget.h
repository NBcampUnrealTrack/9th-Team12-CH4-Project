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
	/** Screen 체력바가 따라가는 몬스터. 지정하지 않으면 테스트 위젯 크기를 유지한다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy Health Bar|Distance")
	void SetTargetActor(AActor* InTargetActor);

	UFUNCTION(BlueprintCallable, Category = "Enemy Health Bar")
	void SetHealth(float CurrentHealth, float MaxHealth, bool bIsCritical = false);

	UFUNCTION(BlueprintCallable, Category = "Enemy Health Bar")
	void ShowHealthDelta(float HealthDelta, bool bIsCritical);

	UFUNCTION(BlueprintCallable, Category = "Enemy Health Bar")
	void SetMonsterName(const FText& InMonsterName);


	UFUNCTION(BlueprintCallable, Category = "Enemy Health Bar")
		void SetHealthPercent(float InHealthPercent);
	UFUNCTION(BlueprintPure, Category = "Enemy Health Bar")
	float GetTargetHealthPercent() const { return TargetHealthPercent; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar|Distance")
	bool bEnableDistanceScaling = true;

	/** 카메라에서 이 거리(cm)일 때 배율 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar|Distance", meta = (ClampMin = "1.0"))
	float ReferenceDistance = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar|Distance", meta = (ClampMin = "0.01"))
	float MinDistanceScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Health Bar|Distance", meta = (ClampMin = "0.01"))
	float MaxDistanceScale = 1.f;

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
	void UpdateDistanceScale();

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> TargetActor;

	FVector2D BaseRenderScale = FVector2D(1.f, 1.f);
	bool bCapturedBaseRenderScale = false;

	void RemoveFeedbackAt(int32 Index);

	float DamageDelayRemaining = 0.0f;
	float LastCurrentHealth = 0.0f;
	bool bHasHealthSample = false;

	UPROPERTY(Transient)
	TArray<FTDHealthFeedbackEntry> ActiveFeedbackEntries;
};
