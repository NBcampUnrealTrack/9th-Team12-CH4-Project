// Fill out your copyright notice in the Description page of Project Settings.


#include "TDHudNavButtonDA.h"

FButtonStyle UTDHudNavButtonDA::GetStyle(bool /*bSelected*/) const
{
	// 선택 상태는 메뉴 로직에서만 사용합니다.
	// 시각 상태는 UButton의 Normal/Hovered/Pressed 동작을 그대로 유지합니다.
	return DefaultStyle;
	
}
