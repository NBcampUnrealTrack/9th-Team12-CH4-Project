#pragma once

#include "CoreMinimal.h"
#include "TDBossPattern.generated.h"

class UNiagaraSystem;

/** 판정 모양. 박스는 정면 방향으로 회전한다. */
UENUM(BlueprintType)
enum class ETDBossHitShape : uint8
{
	Box,
	Sphere
};

/** 패턴에 딸린 이동. None 은 제자리 판정(파도 장판). 나머지는 2단계에서 구현한다. */
UENUM(BlueprintType)
enum class ETDBossMotion : uint8
{
	None,
	Dash,        // 돌진 물기
	Burrow,      // 잠수 → 솟구침
	Projectile   // 물대포
};

/** 패턴의 3박자. 모든 패턴이 이 순서를 강제로 따른다. */
UENUM(BlueprintType)
enum class ETDBossPatternPhase : uint8
{
	None,
	Telegraph,   // 선딜: 예고 표시, 판정 없음
	Strike,      // 타격: 판정 발생
	Recovery     // 후딜: 빈틈, 받는 피해 증가
};

/** 전 머신에 방송되는 보스 사건. BP 가 연출·UI 를 붙이는 지점. */
UENUM(BlueprintType)
enum class ETDBossEvent : uint8
{
	Entrance,          // 입장 연출 시작
	PatternTelegraph,  // Param = 패턴 번호
	PatternStrike,
	PatternRecovery,
	PatternEnd,
	Phase2,
	Enrage,
	Summon,            // Param = 소환 수
	Reset,             // 리시 귀환
	Death
};

/**
 * 보스 패턴 한 종류의 명세. 배열 순서가 곧 PatternIndex 다.
 * 시간 세 칸이 3박자를 이루고, 페이즈 2·분노는 보스 전역 배율로 이 값을 줄인다.
 */
USTRUCT(BlueprintType)
struct FTDBossPatternSpec
{
	GENERATED_BODY()

	/** 로그·BP 구분용 이름. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Name = TEXT("Pattern");

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETDBossMotion Motion = ETDBossMotion::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETDBossHitShape Shape = ETDBossHitShape::Box;

	/** 박스 절반 크기. X 가 정면. 파도 장판은 X 를 길게. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Shape == ETDBossHitShape::Box"))
	FVector BoxExtent = FVector(300.f, 150.f, 150.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Shape == ETDBossHitShape::Sphere", ClampMin = "0"))
	float SphereRadius = 300.f;

	/** 보스 중심에서 판정 중심까지의 정면 거리. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ForwardOffset = 300.f;

	// ── 3박자 ──

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float TelegraphTime = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float StrikeTime = 0.3f;

	/** 0 보다 크면 StrikeTime 동안 이 간격으로 반복 판정한다(한 대상 1회). 돌진·광선용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float StrikeInterval = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float RecoveryTime = 1.0f;

	// ── 수치 ──

	/** 공격력 배율. 1.0 = 평타와 같은 공식. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float DamageScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float Cooldown = 6.f;

	/** 선택 조건: 대상까지의 거리 범위. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float MinDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float MaxDistance = 100000.f;

	/** 선택 가중치. 클수록 자주 나온다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float Weight = 1.f;

	/** 타격 순간 화면 흔들림 크기. 0 = 없음, 1 = 보통, 3 = 큼. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float ShakeScale = 1.f;
	
	// ── VFX (희진님이 채운다. 비우면 없음) ──

	/** 선딜 동안 판정 자리에 띄우는 루프 이펙트. 타격 시작에 지운다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> TelegraphVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0.01"))
	float TelegraphVFXScale = 1.f;

	/** 타격 순간 판정 자리에 한 번 터지는 이펙트. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> StrikeVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0.01"))
	float StrikeVFXScale = 1.f;
};