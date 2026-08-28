// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDWindowBaseWidget.generated.h"

class UTDWindowFrameWidget;

/**
 * Inventory, Character, Equipment, Quest처럼 WindowLayer에 표시되는 창의 공통 베이스다.
 *
 * 파생 WBP에는 UTDWindowFrameWidget 타입의 위젯을 하나 배치하고
 * 반드시 "WindowFrame"이라는 이름으로 지정해야 한다.
 */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDWindowBaseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 창 제목을 변경하고 현재 프레임에도 즉시 반영한다. */
	UFUNCTION(BlueprintCallable, Category = "Window")
	void SetWindowTitle(const FText& InTitle);

	/** 창 닫기 전 추가 처리가 필요하면 Blueprint 또는 C++에서 재정의할 수 있다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Window")
	void CloseWindow();
	virtual void CloseWindow_Implementation();

	UFUNCTION(BlueprintPure, Category = "Window")
	UTDWindowFrameWidget* GetWindowFrame() const { return WindowFrame; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** WBP의 기본 제목. 디자이너나 생성 직후 SetWindowTitle로 변경할 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Window", meta = (ExposeOnSpawn = "true"))
	FText WindowTitle;

	/** 프레임 제목 표시줄을 잡아 WindowLayer 위에서 이동할 수 있는지 여부다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Window")
	bool bWindowDraggingEnabled = true;

	/** 파생 WBP에 배치한 WBP_WindowsFrame의 변수 이름과 반드시 일치해야 한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTDWindowFrameWidget> WindowFrame;

private:
	UFUNCTION()
	void HandleCloseRequested();

	void ApplyWindowSettings();
};
