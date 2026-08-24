#include "Core/TDGameInstance.h"

#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void UTDGameInstance::Init()
{
	Super::Init();

	// 서버·클라이언트 양쪽에서 한 번씩 불려야 한다.
	UAbilitySystemGlobals::Get().InitGlobalData();
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
