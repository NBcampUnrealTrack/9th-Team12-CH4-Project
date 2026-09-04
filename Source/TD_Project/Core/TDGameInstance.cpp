#include "Core/TDGameInstance.h"

#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Settings/TDGameUserSettings.h"
#include "UObject/UObjectGlobals.h"

void UTDGameInstance::Init()
{
	Super::Init();

	// 서버·클라이언트 양쪽에서 한 번씩 불려야 한다.
	UAbilitySystemGlobals::Get().InitGlobalData();

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UTDGameInstance::HandlePostLoadMap);
}

void UTDGameInstance::Shutdown()
{
	// 게임 인스턴스와 수명이 같긴 하지만, 전역 델리게이트에 건 것은 명시적으로 뗀다.
	// 남겨두면 종료 순서에 따라 파괴된 객체를 부르게 될 수 있다.
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	Super::Shutdown();
}

void UTDGameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// 데디케이티드 서버에는 화면도 소리도 없다. 옵션은 클라이언트만의 것이다.
	if (IsDedicatedServerInstance())
	{
		return;
	}

	if (UTDGameUserSettings* Settings = UTDGameUserSettings::Get())
	{
		Settings->ApplyAllSettings();
	}
	else
	{
		// GameUserSettingsClassName 이 DefaultEngine.ini 에 없으면 엔진 기본 클래스가 쓰여
		// 캐스팅이 실패한다. 그 경우 볼륨·접근성 옵션이 통째로 동작하지 않는다.
		UE_LOG(LogTemp, Warning,
			TEXT("옵션: UTDGameUserSettings 를 가져오지 못했다. "
			     "DefaultEngine.ini 의 GameUserSettingsClassName 을 확인할 것."));
	}
}

bool UTDGameInstance::ConnectToServer(const FString& Address)
{
	const FString TrimmedAddress = Address.TrimStartAndEnd();
	if (TrimmedAddress.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ConnectToServer: 주소가 비어 있다."));
		return false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("ConnectToServer: World 가 없다."));
		return false;
	}

	// ClientTravel 은 로컬 컨트롤러가 있어야 한다. 데디케이티드 서버에는 없으므로
	// 이 함수는 클라이언트에서만 의미가 있다.
	APlayerController* LocalController = World->GetFirstPlayerController();
	if (LocalController == nullptr || !LocalController->IsLocalController())
	{
		UE_LOG(LogTemp, Warning, TEXT("ConnectToServer: 로컬 PlayerController 가 없다."));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("서버 접속 시도: %s"), *TrimmedAddress);

	LocalController->ClientTravel(TrimmedAddress, ETravelType::TRAVEL_Absolute);
	return true;
}
