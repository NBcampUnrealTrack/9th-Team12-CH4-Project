#pragma once

#include "CoreMinimal.h"

/**
 * BB_Boss 의 키 이름. 에디터에서 만드는 블랙보드 키와 철자가 같아야 한다.
 * 문자열을 노드마다 적으면 오타 하나로 조용히 안 돌아가므로 한 곳에 둔다.
 */
namespace TDBossBB
{
	const FName Target(TEXT("Target"));             // Object(Actor): 현재 대상
	const FName Distance(TEXT("Distance"));         // Float: 대상까지 XY 중심 거리
	const FName Phase(TEXT("Phase"));               // Int: 보스 페이즈
	const FName NextPattern(TEXT("NextPattern"));   // Int: 선택된 패턴 번호
	const FName bBusy(TEXT("bBusy"));               // Bool: 입장·전환·패턴 진행 중
	const FName bFightActive(TEXT("bFightActive")); // Bool: 전투 중
	const FName bReturning(TEXT("bReturning"));     // Bool: 리시 귀환 중
	const FName HomeLocation(TEXT("HomeLocation")); // Vector: 집
	const FName bPatternReady(TEXT("bPatternReady"));
}