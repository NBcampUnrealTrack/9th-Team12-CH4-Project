#include "UI/InGame/TDChatBubbleWidget.h"

#include "Camera/PlayerCameraManager.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "UI/Common/Typography/TDTextBlock.h"
#include "UI/ViewModel/TDChatSubsystem.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

#include "Components/SizeBox.h"
#include "Components/CanvasPanelSlot.h"

TSharedRef<SWidget> UTDChatBubbleWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	if (!ChatBubble)
	{
		// A small, always-visible root keeps ticking even while the bubble is hidden.
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ChatBubbleRoot"));
		Root->SetWidthOverride(1.f);
		Root->SetHeightOverride(1.f);
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ChatBubbleCanvas"));
		Root->SetContent(Canvas);
		ChatBubble = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChatBubbleBorder"));
		ChatBubble->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 8.f));
		ChatBubbleText = WidgetTree->ConstructWidget<UTDTextBlock>(UTDTextBlock::StaticClass(), TEXT("ChatBubbleMessage"));
		ChatBubbleText->SetVisibility(ESlateVisibility::HitTestInvisible);
		ChatBubbleText->SetJustification(ETextJustify::Center);
		ChatBubble->SetContent(ChatBubbleText);
		UCanvasPanelSlot* BubbleSlot = Canvas->AddChildToCanvas(ChatBubble);
		BubbleSlot->SetAnchors(FAnchors(0.5f, 0.f));
		BubbleSlot->SetAlignment(FVector2D(0.5f, 1.f));
		BubbleSlot->SetAutoSize(true);
		WidgetTree->RootWidget = Root;
		ApplyChatBubbleStyle();
		HideChatBubble();
	}
	return Super::RebuildWidget();
}

void UTDChatBubbleWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyChatBubbleStyle();
}

void UTDChatBubbleWidget::ApplyChatBubbleStyle()
{
	if (ChatBubble) { ChatBubble->SetPadding(ChatBubblePadding); ChatBubble->SetBrushColor(ChatBubbleBackground); }
	if (ChatBubbleText)
	{
		ChatBubbleText->SetTextStyleRole(ChatBubbleTextStyle);
		const FLinearColor TextColor = ActiveChatBubbleChannel == ETDChatChannel::Party ? ChatBubblePartyTextColor
			: ActiveChatBubbleChannel == ETDChatChannel::Whisper ? ChatBubbleWhisperTextColor : ChatBubbleTextColor;
		ChatBubbleText->SetTypographyColorOverride(TextColor);
		ChatBubbleText->SetWrapTextAt(FMath::Max(40.f, ChatBubbleWrapWidth));
	}
}

void UTDChatBubbleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	HideChatBubble();
	RefreshChatSource();
}

void UTDChatBubbleWidget::NativeDestruct()
{
	UnbindChatSource();
	HideChatBubble();
	Super::NativeDestruct();
}

void UTDChatBubbleWidget::RefreshChatSource()
{
	if (IsDesignTime()) return;
	APlayerController* Controller = GetOwningPlayer();
	if (!Controller) Controller = UGameplayStatics::GetPlayerController(this, 0);
	ULocalPlayer* LocalPlayer = Controller && Controller->IsLocalController() ? Controller->GetLocalPlayer() : nullptr;
	UTDChatSubsystem* NewSource = LocalPlayer ? LocalPlayer->GetSubsystem<UTDChatSubsystem>() : nullptr;
	if (ChatSource.Get() == NewSource) return;
	UnbindChatSource();
	HideChatBubble();
	ChatSource = NewSource;
	if (NewSource)
	{
		NewSource->OnMessageReceived.AddUObject(this, &ThisClass::HandleChatMessage);
		NewSource->OnHistoryChanged.AddUObject(this, &ThisClass::HandleChatHistoryChanged);
	}
}

void UTDChatBubbleWidget::UnbindChatSource()
{
	if (UTDChatSubsystem* Source = ChatSource.Get())
	{
		Source->OnMessageReceived.RemoveAll(this);
		Source->OnHistoryChanged.RemoveAll(this);
	}
	ChatSource.Reset();
}

void UTDChatBubbleWidget::HandleChatHistoryChanged()
{
	if (!ChatSource.IsValid() || ChatSource->GetMessages().IsEmpty()) HideChatBubble();
}

void UTDChatBubbleWidget::HandleChatMessage(const FTDChatMessage& Entry)
{
	// This is a local presentation of an already-authorized receipt: never multicast
	// private chat through the character or replay chat history on newly visible pawns.
	if (Entry.Channel != ETDChatChannel::All && Entry.Channel != ETDChatChannel::Party
		&& Entry.Channel != ETDChatChannel::Whisper) return;
	const APawn* Pawn = TargetPawn.Get();
	const APlayerState* State = Pawn ? Pawn->GetPlayerState() : nullptr;
	if (!State || Entry.SenderName.IsEmpty() || State->GetPlayerName() != Entry.SenderName
		|| Entry.Message.IsEmpty() || Entry.bSendFailure || !ChatBubble || !ChatBubbleText
		|| ChatBubbleDuration <= 0.f || !GetWorld()) return;
	MessageSenderName = Entry.SenderName;
	ActiveChatBubbleChannel = Entry.Channel;
	ApplyChatBubbleStyle();
	ChatBubbleText->SetText(FText::FromString(Entry.Message));
	UpdateChatBubbleVisibility();
	GetWorld()->GetTimerManager().SetTimer(ChatBubbleTimer, this, &ThisClass::HideChatBubble, ChatBubbleDuration, false);
}

void UTDChatBubbleWidget::UpdateChatBubbleVisibility()
{
	if (!ChatBubble || !ChatBubbleText) return;
	bool bShow = false;
	if (!ChatBubbleText->GetText().IsEmpty())
	{
		APlayerController* Controller = GetOwningPlayer();
		if (!Controller) Controller = UGameplayStatics::GetPlayerController(this, 0);
		const APawn* Viewer = Controller && Controller->IsLocalController() ? Controller->GetPawn() : nullptr;
		const APawn* Speaker = TargetPawn.Get();
		const APlayerState* SpeakerState = Speaker ? Speaker->GetPlayerState() : nullptr;
		const float MaxDistance = FMath::Max(0.f, ChatBubbleMaxDistance);
		bShow = Viewer && Speaker && SpeakerState && SpeakerState->GetPlayerName() == MessageSenderName && (Viewer == Speaker ||
			FVector::DistSquared(Viewer->GetActorLocation(), Speaker->GetActorLocation()) <= FMath::Square(MaxDistance));
	}
	const ESlateVisibility BubbleVisibility = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (ChatBubble->GetVisibility() != BubbleVisibility) ChatBubble->SetVisibility(BubbleVisibility);
}

void UTDChatBubbleWidget::HideChatBubble()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ChatBubbleTimer);
	if (ChatBubble) ChatBubble->SetVisibility(ESlateVisibility::Collapsed);
	if (ChatBubbleText) ChatBubbleText->SetText(FText::GetEmpty());
	MessageSenderName.Reset();
}

void UTDChatBubbleWidget::SetTargetPawn(APawn* InTargetPawn)
{
	if (!bCapturedBaseRenderScale)
	{
		// WBP 에서 준 기본 배율을 기억해 둔다. 거리 배율은 여기에 곱한다.
		BaseRenderScale = GetRenderTransform().Scale;
		bCapturedBaseRenderScale = true;
	}

	HideChatBubble();
	TargetPawn = InTargetPawn;
	UpdateDistanceScale();
}

void UTDChatBubbleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshChatSource();

	UpdateDistanceScale();
	UpdateChatBubbleVisibility();
}

void UTDChatBubbleWidget::UpdateDistanceScale()
{
	if (IsDesignTime() || !bCapturedBaseRenderScale)
	{
		return;
	}

	float Scale = 1.f;
	APlayerController* Player = GetOwningPlayer();
	if (Player == nullptr)
	{
		// 위젯을 월드가 만들면 소유 플레이어가 비는 경우가 있다. 클라이언트당 로컬 플레이어는 한 명이다.
		Player = UGameplayStatics::GetPlayerController(this, 0);
	}

	const APawn* Pawn = TargetPawn.Get();
	if (Pawn != nullptr && Player != nullptr && Player->IsLocalController() && Player->PlayerCameraManager != nullptr)
	{
		const float Distance = FVector::Distance(
			Player->PlayerCameraManager->GetCameraLocation(), Pawn->GetActorLocation());
		const float SafeMin = FMath::Max(0.01f, MinDistanceScale);
		const float SafeMax = FMath::Max(SafeMin, MaxDistanceScale);
		Scale = FMath::Clamp(FMath::Max(1.f, ReferenceDistance)
			/ FMath::Max(1.f, Distance), SafeMin, SafeMax);
	}

	const FVector2D NewScale = BaseRenderScale * Scale;
	if (!GetRenderTransform().Scale.Equals(NewScale))
	{
		SetRenderScale(NewScale);
	}
}
