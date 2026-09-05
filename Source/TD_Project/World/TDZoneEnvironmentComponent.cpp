#include "World/TDZoneEnvironmentComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Data/TDZoneEnvironmentRow.h"
#include "Engine/DataTable.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Player/TDPlayerState.h"
#include "Settings/TDZoneSettings.h"

UTDZoneEnvironmentComponent::UTDZoneEnvironmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 컨트롤러에 붙지만 서버에는 접속자 수만큼 컨트롤러가 있다. 복제할 것이 없으므로
	// 네트워크에서 뺀다 — 이 컴포넌트가 만드는 것은 전부 자기 화면에서만 의미가 있다.
	SetIsReplicatedByDefault(false);
}

void UTDZoneEnvironmentComponent::BeginPlay()
{
	Super::BeginPlay();

	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (Controller == nullptr || !Controller->IsLocalController())
	{
		// 남의 컨트롤러다(서버에 있는 다른 접속자의 것). 전부가 라이팅을 만지면
		// 마지막에 처리된 사람의 존이 이긴다.
		SetComponentTickEnabled(false);
		return;
	}

	// 조명을 건드리기 전에 원래 값을 잡아 둔다. 레벨 액터는 이 시점에 이미 있다.
	CaptureLightingDefaults();

	TryBindPlayerState();
}

bool UTDZoneEnvironmentComponent::TryBindPlayerState()
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	ATDPlayerState* PlayerState = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;

	if (PlayerState == nullptr)
	{
		return false;
	}

	PlayerState->OnZoneChanged.AddDynamic(this, &UTDZoneEnvironmentComponent::HandleZoneChanged);
	bWaitingForPlayerState = false;

	// 구독을 건 시점에 이미 존이 정해져 있을 수 있다. 접속하면서 서버가 존을 넣어 주는데,
	// 그 복제가 이 컴포넌트의 BeginPlay 보다 먼저 도착하면 알림을 놓친다.
	if (PlayerState->GetCurrentZoneId().IsValid())
	{
		ApplyZone(PlayerState->GetCurrentZoneId());
	}

	return true;
}

void UTDZoneEnvironmentComponent::HandleZoneChanged(FGameplayTag NewZoneId)
{
	ApplyZone(NewZoneId);
}

void UTDZoneEnvironmentComponent::ReapplyCurrentZone()
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	const ATDPlayerState* PlayerState = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;

	if (PlayerState != nullptr && PlayerState->GetCurrentZoneId().IsValid())
	{
		ApplyZone(PlayerState->GetCurrentZoneId());
	}
}

void UTDZoneEnvironmentComponent::ApplyZone(FGameplayTag ZoneId)
{
	const UTDZoneSettings* Settings = GetDefault<UTDZoneSettings>();
	if (Settings == nullptr)
	{
		return;
	}

	// 카메라 기본값은 BP 에서 읽는다. Pawn 이 아직 없으면 못 읽지만, 그때는 카메라를
	// 적용할 대상도 없어서 문제가 되지 않는다 — Possess 때 다시 들어온다.
	CaptureCameraDefaults(FindSpringArm());

	// 존이 지정하지 않은 항목은 시작 시 잡아 둔 값으로 **되돌아간다.**
	//
	// 카메라와 라이팅이 같은 규칙이다. 라이팅만 "건드리지 않는다" 로 두면 이전 존이
	// 바꿔 놓은 조명이 그대로 남아, 마을로 돌아와도 던전 조명이 유지된다.
	FTDZoneCameraSettings Camera = CapturedCamera;
	FTDZoneLightingSettings Lighting = CapturedLighting;

	// 존 정의 → 표현 에셋. 클라이언트도 이 테이블을 갖고 있다.
	if (UDataTable* Table = Settings->ZoneEnvironmentTable.LoadSynchronous())
	{
		const FTDZoneEnvironmentRow* Found = nullptr;

		// ZoneId 열로 찾는다. RowName 이 아니다 — 태그 계층 조회 때문에 별도 열로 두었다.
		Table->ForeachRow<FTDZoneEnvironmentRow>(TEXT("ApplyZone"),
			[&Found, ZoneId](const FName&, const FTDZoneEnvironmentRow& Row)
			{
				if (Found == nullptr && Row.ZoneId == ZoneId)
				{
					Found = &Row;
				}
			});

		if (Found != nullptr)
		{
			// 존을 옮기는 순간에 한 번만 읽으므로 동기 로드를 허용한다. 비동기로 하면
			// 로드가 끝날 때까지 이전 존의 화면이 남아 이동이 반영되지 않은 것처럼 보인다.
			if (const UTDZoneEnvironmentData* Data = Found->EnvironmentData.LoadSynchronous())
			{
				if (Data->bOverride_Camera)
				{
					Camera = Data->Camera;
				}

				if (Data->bOverride_Lighting)
				{
					Lighting = Data->Lighting;
				}
			}
		}
	}

	TargetCamera = Camera;
	RemainingBlendTime = FMath::Max(0.f, Camera.BlendTime);

	if (RemainingBlendTime <= 0.f)
	{
		// 즉시 적용. 아래 Tick 과 같은 일을 한 번에 한다.
		if (USpringArmComponent* SpringArm = FindSpringArm())
		{
			SpringArm->TargetArmLength = TargetCamera.ArmLength;

			FRotator Rotation = SpringArm->GetRelativeRotation();
			Rotation.Pitch = TargetCamera.Pitch;
			SpringArm->SetRelativeRotation(Rotation);

			if (UCameraComponent* CameraComponent =
				SpringArm->GetOwner()->FindComponentByClass<UCameraComponent>())
			{
				CameraComponent->SetFieldOfView(TargetCamera.FieldOfView);
			}
		}
	}

	// 라이팅은 보간하지 않는다. 조명은 카메라와 달리 조금씩 변하는 중간 상태가
	// 어색하고, 존 이동은 대개 순간이동이라 화면이 어차피 한 번 끊긴다.
	ApplyLighting(Lighting);
}

void UTDZoneEnvironmentComponent::CaptureCameraDefaults(const USpringArmComponent* SpringArm)
{
	if (bCameraCaptured || SpringArm == nullptr)
	{
		return;
	}

	CapturedCamera.ArmLength = SpringArm->TargetArmLength;
	CapturedCamera.Pitch = SpringArm->GetRelativeRotation().Pitch;

	if (const UCameraComponent* Camera =
		SpringArm->GetOwner()->FindComponentByClass<UCameraComponent>())
	{
		CapturedCamera.FieldOfView = Camera->FieldOfView;
	}

	// BlendTime 은 캡처하지 않는다. 스프링암에 그런 값이 없고, 존이 지정하지 않았을 때
	// 얼마나 부드럽게 돌아갈지는 연출 판단이라 구조체 기본값(0.5초)을 쓴다.
	bCameraCaptured = true;

	UE_LOG(LogTemp, Log, TEXT("존 카메라 기본값을 BP 에서 읽었다: 거리 %.0f, 각도 %.0f, FOV %.0f"),
		CapturedCamera.ArmLength, CapturedCamera.Pitch, CapturedCamera.FieldOfView);
}

void UTDZoneEnvironmentComponent::CaptureLightingDefaults()
{
	if (bLightingCaptured)
	{
		return;
	}

	CacheLightActors();
	bLightingCaptured = true;

	// 레벨에 없는 액터는 구조체 기본값이 그대로 남는다. 어차피 적용할 때도
	// 그 항목을 건너뛰므로 값이 무엇이든 쓰이지 않는다.
	if (const ADirectionalLight* Sun = CachedSunLight.Get())
	{
		CapturedLighting.SunRotation = Sun->GetActorRotation();

		if (const UDirectionalLightComponent* Light =
			Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			CapturedLighting.SunColor = Light->GetLightColor();
			CapturedLighting.SunIntensity = Light->Intensity;
		}
	}

	if (const ASkyLight* Sky = CachedSkyLight.Get())
	{
		if (const USkyLightComponent* Light = Sky->GetLightComponent())
		{
			CapturedLighting.SkyLightIntensity = Light->Intensity;
		}
	}

	if (const AExponentialHeightFog* Fog = CachedFog.Get())
	{
		if (const UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			CapturedLighting.FogDensity = FogComponent->FogDensity;
			CapturedLighting.FogColor = FogComponent->FogInscatteringLuminance;
		}
	}

	// 값을 함께 찍는다. 존을 옮겼다 돌아왔을 때 이 값으로 복귀했는지 눈으로 대조할 수 있다.
	UE_LOG(LogTemp, Log,
		TEXT("존 라이팅 기본값을 레벨에서 읽었다: 태양 %s(각도 %s, 세기 %.2f) / "
			 "스카이 %s(세기 %.2f) / 안개 %s(밀도 %.4f)"),
		CachedSunLight.IsValid() ? TEXT("있음") : TEXT("없음"),
		*CapturedLighting.SunRotation.ToCompactString(), CapturedLighting.SunIntensity,
		CachedSkyLight.IsValid() ? TEXT("있음") : TEXT("없음"),
		CapturedLighting.SkyLightIntensity,
		CachedFog.IsValid() ? TEXT("있음") : TEXT("없음"),
		CapturedLighting.FogDensity);
}

USpringArmComponent* UTDZoneEnvironmentComponent::FindSpringArm() const
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;

	// BP_Player 에서 만든 컴포넌트도 클래스로 찾을 수 있다. 이름에 의존하지 않는 이유는
	// 블루프린트에서 이름을 바꾸면 조용히 못 찾게 되기 때문이다.
	return Pawn != nullptr ? Pawn->FindComponentByClass<USpringArmComponent>() : nullptr;
}

void UTDZoneEnvironmentComponent::CacheLightActors()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	bLightActorsCached = true;

	// 각 종류의 첫 번째를 쓴다. 월드에 하나만 유효한 액터들이라 여러 개 있으면
	// 애초에 레벨 구성이 잘못된 것이다(D49).
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		CachedSunLight = *It;
		break;
	}

	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		CachedSkyLight = *It;
		break;
	}

	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		CachedFog = *It;
		break;
	}
}

void UTDZoneEnvironmentComponent::ApplyLighting(const FTDZoneLightingSettings& Settings)
{
	// BeginPlay 의 기본값 캡처가 이미 찾아 두었을 것이다. 그쪽이 어떤 이유로
	// 건너뛰었을 때를 위한 방어다.
	if (!bLightActorsCached)
	{
		CacheLightActors();
	}

	// 레벨에 없는 액터는 그 항목만 건너뛴다. 아트가 아직 배치하지 않았어도
	// 카메라는 정상 동작해야 한다.
	if (ADirectionalLight* Sun = CachedSunLight.Get())
	{
		Sun->SetActorRotation(Settings.SunRotation);

		if (UDirectionalLightComponent* Light =
			Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			Light->SetLightColor(Settings.SunColor);
			Light->SetIntensity(Settings.SunIntensity);
		}
	}

	if (ASkyLight* Sky = CachedSkyLight.Get())
	{
		if (USkyLightComponent* Light = Sky->GetLightComponent())
		{
			Light->SetIntensity(Settings.SkyLightIntensity);

			// 색·강도를 바꿔도 캡처를 다시 하지 않으면 화면에 반영되지 않는다.
			Light->RecaptureSky();
		}
	}

	if (AExponentialHeightFog* Fog = CachedFog.Get())
	{
		if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			FogComponent->SetFogDensity(Settings.FogDensity);
			FogComponent->SetFogInscatteringColor(Settings.FogColor);
		}
	}
}

void UTDZoneEnvironmentComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 접속 순서상 컨트롤러가 PlayerState 보다 먼저 준비될 수 있다. 잡을 때까지 다시 시도한다.
	if (bWaitingForPlayerState)
	{
		TryBindPlayerState();
		return;
	}

	if (RemainingBlendTime <= 0.f)
	{
		return;
	}

	USpringArmComponent* SpringArm = FindSpringArm();
	if (SpringArm == nullptr)
	{
		// Pawn 이 아직 없다(캐릭터 선택 전이거나 부활 중). 스폰되면 컨트롤러가
		// ReapplyCurrentZone 을 불러 다시 시작한다.
		return;
	}

	// 남은 시간을 기준으로 비율을 잡는다. 고정 속도로 보간하면 BlendTime 이 지나도
	// 목표에 도달하지 못한다.
	const float Alpha = FMath::Clamp(DeltaTime / RemainingBlendTime, 0.f, 1.f);

	SpringArm->TargetArmLength =
		FMath::Lerp(SpringArm->TargetArmLength, TargetCamera.ArmLength, Alpha);

	FRotator Rotation = SpringArm->GetRelativeRotation();
	Rotation.Pitch = FMath::Lerp(Rotation.Pitch, TargetCamera.Pitch, Alpha);
	SpringArm->SetRelativeRotation(Rotation);

	if (UCameraComponent* Camera = SpringArm->GetOwner()->FindComponentByClass<UCameraComponent>())
	{
		Camera->SetFieldOfView(
			FMath::Lerp(Camera->FieldOfView, TargetCamera.FieldOfView, Alpha));
	}

	RemainingBlendTime -= DeltaTime;
}
