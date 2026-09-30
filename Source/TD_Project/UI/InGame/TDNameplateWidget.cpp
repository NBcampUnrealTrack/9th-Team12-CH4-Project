#include "UI/InGame/TDNameplateWidget.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

void UTDNameplateWidget::SetTargetPawn(APawn* InTargetPawn)
{
	if (!bCapturedBaseRenderScale)
	{
		// WBP 에서 준 기본 배율을 기억해 둔다. 거리 배율은 여기에 곱한다.
		BaseRenderScale = GetRenderTransform().Scale;
		bCapturedBaseRenderScale = true;
	}

	TargetPawn = InTargetPawn;
	DisplayedName.Reset();
	UpdateName();
	UpdateDistanceScale();
}

void UTDNameplateWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateName();
	UpdateDistanceScale();
}

void UTDNameplateWidget::UpdateName()
{
	if (NameText == nullptr)
	{
		return;
	}

	const APawn* Pawn = TargetPawn.Get();
	const APlayerState* State = Pawn != nullptr ? Pawn->GetPlayerState() : nullptr;
	const FString NewName = State != nullptr ? State->GetPlayerName() : FString();

	if (NewName == DisplayedName)
	{
		return;
	}

	DisplayedName = NewName;
	NameText->SetText(FText::FromString(NewName));

	// 캐릭터를 고르기 전이거나 PlayerState 가 아직 안 온 동안은 빈 칸이 뜨지 않게 감춘다.
	// 감추는 것은 글자 쪽이다 — 이 위젯을 Collapsed 로 두면 Slate 가 틱을 멈춰
	// 이름이 도착해도 다시 켜 줄 코드가 돌지 않는다.
	NameText->SetVisibility(NewName.IsEmpty()
		? ESlateVisibility::Hidden
		: ESlateVisibility::HitTestInvisible);
}

void UTDNameplateWidget::UpdateDistanceScale()
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
