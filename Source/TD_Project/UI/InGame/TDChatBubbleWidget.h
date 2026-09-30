#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Chat/TDChatTypes.h"
#include "UI/Common/Typography/TDTypographyThemeDA.h"
#include "TDChatBubbleWidget.generated.h"

class UBorder;
class UTDTextBlock;
class UTDChatSubsystem;
struct FTDChatMessage;

/** 서버가 승인한 채팅을 로컬 화면의 해당 캐릭터 위에 표시한다. 이름표와 독립적이다. */
UCLASS(Blueprintable)
class TD_PROJECT_API UTDChatBubbleWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "TD|ChatBubble")
	void SetTargetPawn(APawn* InTargetPawn);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 말풍선 표시 시간. 이름표와 독립적으로 동작한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|ChatBubble", meta = (ClampMin = "0.0"))
	float ChatBubbleDuration = 5.f;

	/** 보는 플레이어의 Pawn과 말하는 Pawn 사이 거리(cm). 자기 말풍선은 항상 표시한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|ChatBubble", meta = (ClampMin = "0.0", Units = "cm"))
	float ChatBubbleMaxDistance = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|ChatBubble", meta = (ClampMin = "40.0"))
	float ChatBubbleWrapWidth = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|ChatBubble")
	FMargin ChatBubblePadding = FMargin(12.f, 8.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|ChatBubble")
	FLinearColor ChatBubbleBackground = FLinearColor(0.025f, 0.025f, 0.035f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|ChatBubble")
	FLinearColor ChatBubbleTextColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|ChatBubble")
	FLinearColor ChatBubblePartyTextColor = FLinearColor::FromSRGBColor(FColor(96, 165, 250));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|ChatBubble")
	FLinearColor ChatBubbleWhisperTextColor = FLinearColor::FromSRGBColor(FColor(255, 212, 184));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|ChatBubble")
	ETDTextStyleRole ChatBubbleTextStyle = ETDTextStyleRole::BodySmall;
	/** 카메라에서 이 거리(cm)일 때 배율 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|ChatBubble|Distance", meta = (ClampMin = "1.0"))
	float ReferenceDistance = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|ChatBubble|Distance", meta = (ClampMin = "0.01"))
	float MinDistanceScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TD|ChatBubble|Distance", meta = (ClampMin = "0.01"))
	float MaxDistanceScale = 1.f;

private:
	void RefreshChatSource();
	void UnbindChatSource();
	void HandleChatMessage(const FTDChatMessage& Entry);
	void HandleChatHistoryChanged();
	void HideChatBubble();
	void UpdateChatBubbleVisibility();
	void ApplyChatBubbleStyle();

	UPROPERTY(Transient) TObjectPtr<UBorder> ChatBubble;
	UPROPERTY(Transient) TObjectPtr<UTDTextBlock> ChatBubbleText;
	TWeakObjectPtr<UTDChatSubsystem> ChatSource;
	FTimerHandle ChatBubbleTimer;
	ETDChatChannel ActiveChatBubbleChannel = ETDChatChannel::All;

	void UpdateDistanceScale();
	TWeakObjectPtr<APawn> TargetPawn;
	FString MessageSenderName;
	FVector2D BaseRenderScale = FVector2D(1.f, 1.f);
	bool bCapturedBaseRenderScale = false;
};
