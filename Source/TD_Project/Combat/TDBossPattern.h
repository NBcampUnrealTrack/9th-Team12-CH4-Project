#pragma once

#include "CoreMinimal.h"
#include "TDBossPattern.generated.h"

class UNiagaraSystem;

/**
 * 판정 모양. 높이는 전부 **보스 발밑 기준**이다 — 캡슐 중심을 쓰면 키 큰 보스는 판정이
 * 플레이어 머리 위에 떠서 예고 그림과 실제 맞는 범위가 어긋난다.
 */
UENUM(BlueprintType)
enum class ETDBossHitShape : uint8
{
	Box,     // 정면 방향 상자. BoxDirections 로 십자(4)·양방향(2) 등 여러 갈래
	Sphere,  // 원판. 바닥 기준 원기둥이라 예고 원과 맞는 범위가 정확히 같다
	Ring     // 도넛. 안쪽 반지름 ~ 바깥 반지름 사이만 맞는다. 물기둥·물의 폭포
};

/** 패턴에 딸린 이동. None 은 제자리 판정(파도 장판·물기둥). */
UENUM(BlueprintType)
enum class ETDBossMotion : uint8
{
	None,
	Dash,        // 돌진 물기. DashCount 로 연속 돌진
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
	PatternStrike,     // 추가 타격(ExtraStrikes)이 떨어질 때도 한 번씩 더 온다
	PatternRecovery,
	PatternEnd,
	Phase2,
	Enrage,
	Summon,            // Param = 소환 수
	Reset,             // 리시 귀환
	Death
};

/**
 * 판정 영역 하나. 추가 타격(ExtraStrikes)이 쓴다.
 * 패턴 본체(FTDBossPatternSpec)는 같은 칸을 평평하게 들고 있다 — 이미 채워 둔 BP 값을 잃지 않으려고.
 */
USTRUCT(BlueprintType)
struct FTDBossHitArea
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETDBossHitShape Shape = ETDBossHitShape::Ring;

	/** 박스 절반 크기. X 가 정면. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Shape == ETDBossHitShape::Box"))
	FVector BoxExtent = FVector(300.f, 150.f, 150.f);

	/** 박스 갈래 수. 1 = 정면만, 2 = 앞뒤, 4 = 십자. 정면부터 균등하게 돈다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Shape == ETDBossHitShape::Box", ClampMin = "1", ClampMax = "12"))
	int32 BoxDirections = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Shape == ETDBossHitShape::Sphere", ClampMin = "0"))
	float SphereRadius = 300.f;

	/** 도넛 안쪽 반지름. 이 안은 안전지대다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Shape == ETDBossHitShape::Ring", ClampMin = "0"))
	float RingInnerRadius = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Shape == ETDBossHitShape::Ring", ClampMin = "0"))
	float RingOuterRadius = 450.f;

	/** 중심에서 판정 중심까지의 정면 거리. 박스는 갈래마다 그 방향으로 민다. 도넛·원판은 보통 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ForwardOffset = 0.f;
};

/**
 * 본 타격 뒤에 따라오는 추가 타격. 본 타격 순간에 예고를 띄우고 Delay 초 뒤에 떨어진다.
 * 잠수 후 물의 폭포(도넛, 가운데만 안전), 물기둥 여러 겹(반지름 다른 도넛 여러 개) 이 이걸로 만들어진다.
 */
USTRUCT(BlueprintType)
struct FTDBossExtraStrike
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Name = TEXT("Extra");

	/** 본 타격 순간부터 몇 초 뒤에 떨어지나. 잠수는 솟구침 정점이 본 타격이다. 이 동안이 피할 시간. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float Delay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTDBossHitArea Area;

	/** true = 예고를 띄우는 순간의 보스 위치가 중심. false = 본 타격의 중심(잠수면 출현 지점). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCenterOnBoss = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float DamageScale = 1.f;

	/** 중심에서 바깥으로 밀어내는 속도. 도넛·원판은 위로도 크게 띄운다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float Knockback = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float ShakeScale = 1.f;

	/** 이펙트를 몇 개로 나눠 깔지(채우기). 0 = 하나. 박스: 길이 방향 칸 수. 원판·도넛: 가운데 줄 둘레 개수(원판은 중심 +1). 작은 이펙트를 크게 늘리면 흐려지니 여러 개로 채울 때 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0", ClampMax = "32"))
	int32 VFXPointCount = 0;

	/** 도넛 두께를 몇 겹의 원으로 채우나. 1 = 가운데 한 줄. 4 = 안쪽부터 바깥까지 4줄(바깥 줄일수록 지점이 늘어 밀도 유지). 넓은 비·장판용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "1", ClampMax = "8"))
	int32 VFXRingLayers = 1;

	/** 예고·타격 VFX 를 바닥에서 이만큼(cm) 띄워 스폰. 원점이 가운데인 이펙트가 땅에 묻힐 때. 배율과 무관하게 그대로 더한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	float VFXHeightOffset = 0.f;

	/** 세로(Z) 배율만 따로. 물기둥을 더 높게, 파도를 더 두껍게. 판정 크기와는 무관. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0.01"))
	float VFXHeightScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> TelegraphVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0.01"))
	float TelegraphVFXScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> StrikeVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0.01"))
	float StrikeVFXScale = 1.f;

	/** 타격 VFX 를 이 시간 뒤에 끈다(초). 0 = 이펙트가 알아서 끝남. 무한 루프 이펙트를 쓸 때 반드시 준다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0"))
	float StrikeVFXDuration = 0.f;

	/**
	 * 이펙트를 만들 때 기준으로 삼은 크기(cm). 0 이면 자동 배율 없음.
	 * 예: 반지름 100 짜리로 만든 원형 이펙트면 100 → 판정 반지름 450 이면 4.5배로 띄운다.
	 * 원판·도넛은 반지름, 도넛에 둘레 지점(VFXPointCount)을 쓰면 링 두께의 절반, 박스는 X·Y 각각.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0"))
	float VFXReferenceSize = 0.f;
};

/**
 * 보스 패턴 한 종류의 명세. 배열 순서가 곧 PatternIndex 다.
 * 시간 세 칸이 3박자를 이루고, 페이즈 2·분노는 보스 전역 배율로 이 값을 줄인다.
 *
 * 페이즈 2 강화판은 코드가 아니라 **줄을 하나 더 추가**해서 만든다:
 * 강화판에 MinPhase 2, 원본에 MaxPhase 1 을 주면 페이즈에 따라 교체된다.
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

	// ── 본 판정 ──

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETDBossHitShape Shape = ETDBossHitShape::Box;

	/** 박스 절반 크기. X 가 정면. 파도 장판은 X 를 길게. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Shape == ETDBossHitShape::Box"))
	FVector BoxExtent = FVector(300.f, 150.f, 150.f);

	/** 박스 갈래 수. 1 = 정면만, 4 = 십자 파도. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Shape == ETDBossHitShape::Box", ClampMin = "1", ClampMax = "12"))
	int32 BoxDirections = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Shape == ETDBossHitShape::Sphere", ClampMin = "0"))
	float SphereRadius = 300.f;

	/** 도넛 안쪽 반지름(안전지대). 물기둥이면 보스 몸통보다 조금 크게. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Shape == ETDBossHitShape::Ring", ClampMin = "0"))
	float RingInnerRadius = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Shape == ETDBossHitShape::Ring", ClampMin = "0"))
	float RingOuterRadius = 450.f;

	/** 보스 중심에서 판정 중심까지의 정면 거리. 도넛·물기둥은 0. */
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

	// ── 이동 옵션 ──

	/** 돌진 횟수. 2 면 첫 돌진이 끝난 자리에서 대상 쪽으로 다시 조준해 한 번 더. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Motion == ETDBossMotion::Dash", ClampMin = "1", ClampMax = "5"))
	int32 DashCount = 1;

	/** 돌진과 돌진 사이 재조준 시간(초). 이 동안 몸이 대상 쪽으로 돈다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Motion == ETDBossMotion::Dash", ClampMin = "0.05"))
	float DashReaimTime = 0.4f;

	/**
	 * 잠수: 땅속 이동(TelegraphTime 동안, 예고 없음)이 끝난 뒤 출현 자리에 예고를 띄우는 시간.
	 * 출현 자리는 이동이 끝난 순간의 대상 위치라, 실제로 피할 시간은 이것뿐이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Motion == ETDBossMotion::Burrow", ClampMin = "0.05"))
	float EmergeTelegraphTime = 0.7f;

	/** 물대포 탄 수. 0 = 보스 기본 규칙(페이즈 1 은 1발, 페이즈 2 는 ProjectileCountPhase2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Motion == ETDBossMotion::Projectile", ClampMin = "0", ClampMax = "12"))
	int32 ProjectileCount = 0;

	// ── 추가 타격 ──

	/** 본 타격 뒤에 시간차로 떨어지는 판정들. 물의 폭포, 물기둥 여러 겹. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FTDBossExtraStrike> ExtraStrikes;

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

	/** 선택 조건: 페이즈 범위. 페이즈 2 전용은 MinPhase 2, 페이즈 1 전용은 MaxPhase 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 MinPhase = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 MaxPhase = 99;

	/** 선택 가중치. 클수록 자주 나온다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float Weight = 1.f;

	/** 타격 순간 화면 흔들림 크기. 0 = 없음, 1 = 보통, 3 = 큼. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float ShakeScale = 1.f;

	/** 맞은 대상을 판정 중심에서 바깥으로 밀어내는 속도(cm/s). 0 이면 없음. 박스는 위로 0.4배, 원판·도넛은 0.8배, 잠수는 1.2배. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float Knockback = 0.f;

	// ── VFX (희진님이 채운다. 비우면 없음) ──

	/** 선딜 동안 판정 자리에 띄우는 루프 이펙트. 타격 시작에 지운다. 박스는 갈래마다 하나씩. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> TelegraphVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0.01"))
	float TelegraphVFXScale = 1.f;

	/** 타격 순간 판정 자리에 한 번 터지는 이펙트. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> StrikeVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0.01"))
	float StrikeVFXScale = 1.f;

	/** 타격 VFX 를 이 시간 뒤에 끈다(초). 0 = 이펙트가 알아서 끝남. 무한 루프 이펙트를 쓸 때 반드시 준다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0"))
	float StrikeVFXDuration = 0.f;

	/**
	 * 이펙트를 만들 때 기준으로 삼은 크기(cm). 0 이면 자동 배율 없음.
	 * 예: 반지름 100 짜리로 만든 원형 이펙트면 100 → 판정 반지름 450 이면 4.5배로 띄운다.
	 * 원판·도넛은 반지름, 도넛에 둘레 지점(VFXPointCount)을 쓰면 링 두께의 절반, 박스는 X·Y 각각.
	 * 배율과 별개로 나이아가라 User 파라미터 Radius/InnerRadius/Length/Width/Duration 도 늘 넣어 준다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0"))
	float VFXReferenceSize = 0.f;

	/** 이펙트를 몇 개로 나눠 깔지(채우기). 0 = 하나. 박스: 길이 방향 칸 수. 원판·도넛: 가운데 줄 둘레 개수(원판은 중심 +1). 작은 이펙트를 크게 늘리면 흐려지니 여러 개로 채울 때 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0", ClampMax = "32"))
	int32 VFXPointCount = 0;

	/** 도넛 두께를 몇 겹의 원으로 채우나. 1 = 가운데 한 줄. 4 = 안쪽부터 바깥까지 4줄(바깥 줄일수록 지점이 늘어 밀도 유지). 넓은 비·장판용. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "1", ClampMax = "8"))
	int32 VFXRingLayers = 1;

	/** 예고·타격 VFX 를 바닥에서 이만큼(cm) 띄워 스폰. 원점이 가운데인 이펙트가 땅에 묻힐 때. 배율과 무관하게 그대로 더한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	float VFXHeightOffset = 0.f;

	/** 세로(Z) 배율만 따로. 물기둥을 더 높게, 파도를 더 두껍게. 판정 크기와는 무관. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX", meta = (ClampMin = "0.01"))
	float VFXHeightScale = 1.f;
};
