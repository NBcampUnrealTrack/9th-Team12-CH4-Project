// Fill out your copyright notice in the Description page of Project Settings.

#include "TDWindowBaseWidget.h"

#include "UI/Layer/WindowLayer/Common/WindowFrame/TDWindowFrameWidget.h"

void UTDWindowBaseWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// WBP 디자이너에서도 제목과 드래그 설정이 보이도록 적용한다.
	ApplyWindowSettings();
}

void UTDWindowBaseWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bCloseNotified = false;

	if (WindowFrame)
	{
		// 프레임의 닫기 버튼이 바깥 WindowBase 전체를 닫도록 연결한다.
		WindowFrame->OnCloseRequested.AddUniqueDynamic(
			this,
			&ThisClass::HandleCloseRequested);

		// 드래그할 때 중첩된 프레임이 아니라 WindowLayer의 이 창을 이동한다.
		WindowFrame->SetDragTarget(this);
	}

	ApplyWindowSettings();
}

void UTDWindowBaseWidget::NativeDestruct()
{
	NotifyWindowClosed();

	if (WindowFrame)
	{
		WindowFrame->OnCloseRequested.RemoveDynamic(
			this,
			&ThisClass::HandleCloseRequested);
		WindowFrame->SetDragTarget(nullptr);
	}

	Super::NativeDestruct();
}

void UTDWindowBaseWidget::SetWindowTitle(const FText& InTitle)
{
	WindowTitle = InTitle;

	if (WindowFrame)
	{
		WindowFrame->SetTitle(WindowTitle);
	}
}

void UTDWindowBaseWidget::CloseWindow_Implementation()
{
	// Frame만 제거하지 않고 WindowLayer에 들어간 창 전체를 제거한다.
	NotifyWindowClosed();
	RemoveFromParent();
}

void UTDWindowBaseWidget::HandleCloseRequested()
{
	CloseWindow();
}

void UTDWindowBaseWidget::ApplyWindowSettings()
{
	if (!WindowFrame)
	{
		return;
	}

	WindowFrame->SetTitle(WindowTitle);
	WindowFrame->SetDraggingEnabled(bWindowDraggingEnabled);
}

void UTDWindowBaseWidget::NotifyWindowClosed()
{
	if (bCloseNotified)
	{
		return;
	}

	bCloseNotified = true;
	OnWindowClosed.Broadcast(this);
}
