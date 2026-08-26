// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TDHudNavButtonDA.generated.h"

/**
 * 
 */
UCLASS()
class TD_PROJECT_API UTDHudNavButtonDA : public UDataAsset
{
	GENERATED_BODY()
public:
	// Normal, Hovered, Pressed, Disabled를 포함하는 기본 버튼 스타일
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button Style")
		FButtonStyle DefaultStyle;

	// 버튼이 선택된 상태에서 사용할 이미지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button Style")
		FSlateBrush SelectedBrush;

	// 선택 여부에 맞는 완성된 버튼 스타일 반환
	UFUNCTION(BlueprintPure, Category = "Button Style")
		FButtonStyle GetStyle(bool bSelected) const;
	
};
