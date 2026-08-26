// Fill out your copyright notice in the Description page of Project Settings.


#include "TDHudNavButtonDA.h"

FButtonStyle UTDHudNavButtonDA::GetStyle(bool bSelected) const
{
	if (!bSelected)
	{
		return DefaultStyle;
	}

	FButtonStyle Result = DefaultStyle;

	// 선택된 동안 호버하거나 눌러도 선택 이미지 유지
	Result.SetNormal(SelectedBrush);
	Result.SetHovered(SelectedBrush);
	Result.SetPressed(SelectedBrush);

	return Result;
	
}
