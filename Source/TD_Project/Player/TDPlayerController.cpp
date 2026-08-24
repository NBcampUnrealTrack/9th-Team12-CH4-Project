#include "Player/TDPlayerController.h"

#include "Core/TDGameInstance.h"
#include "GameFramework/PlayerState.h"
#include "Items/TDInventoryComponent.h"
#include "Player/TDPlayerState.h"
#include "Stats/TDProgressionComponent.h"

void ATDPlayerController::TDConnect(const FString& Address)
{
	// Exec 명령은 서버에도 전달될 수 있으므로 로컬에서만 처리한다.
	if (!IsLocalController())
	{
		return;
	}

	if (UTDGameInstance* TDGameInstance = Cast<UTDGameInstance>(GetGameInstance()))
	{
		TDGameInstance->ConnectToServer(Address);
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("TDConnect: GameInstance 가 UTDGameInstance 가 아니다. Project Settings 를 확인할 것."));
}

void ATDPlayerController::ClientTravelToServer_Implementation(const FString& Address)
{
	if (!IsLocalController() || Address.IsEmpty())
	{
		return;
	}

	ClientTravel(Address, ETravelType::TRAVEL_Absolute);
}

// ── 개발용 치트 ───────────────────────────────────────────
// UFUNCTION 선언은 전처리기로 감쌀 수 없어 항상 남는다.
// 대신 구현부를 막아 배포 빌드에서는 호출해도 아무 일이 일어나지 않는다.

void ATDPlayerController::ServerDebugGiveItem_Implementation(FName ItemId, int32 Count)
{
#if !UE_BUILD_SHIPPING
	if (PlayerState == nullptr)
	{
		return;
	}

	if (UTDInventoryComponent* Inventory = PlayerState->FindComponentByClass<UTDInventoryComponent>())
	{
		const bool bAdded = Inventory->AddItem(ItemId, Count);
		UE_LOG(LogTemp, Log, TEXT("[치트] '%s' %d개 지급 %s"),
			*ItemId.ToString(), Count, bAdded ? TEXT("성공") : TEXT("실패"));
	}
#endif
}

void ATDPlayerController::ServerDebugSetLevel_Implementation(int32 NewLevel)
{
#if !UE_BUILD_SHIPPING
	if (PlayerState == nullptr)
	{
		return;
	}

	if (UTDProgressionComponent* Progression = PlayerState->FindComponentByClass<UTDProgressionComponent>())
	{
		const bool bChanged = Progression->SetLevel(NewLevel);
		UE_LOG(LogTemp, Log, TEXT("[치트] 레벨 %d 설정 %s"),
			NewLevel, bChanged ? TEXT("성공") : TEXT("실패"));
	}
#endif
}

void ATDPlayerController::ServerDebugSetClass_Implementation(FName NewClassId)
{
#if !UE_BUILD_SHIPPING
	if (PlayerState == nullptr)
	{
		return;
	}

	if (UTDProgressionComponent* Progression = PlayerState->FindComponentByClass<UTDProgressionComponent>())
	{
		const bool bChanged = Progression->SetClassId(NewClassId);
		UE_LOG(LogTemp, Log, TEXT("[치트] 직업 '%s' 설정 %s"),
			*NewClassId.ToString(), bChanged ? TEXT("성공") : TEXT("실패"));
	}
#endif
}

void ATDPlayerController::ServerDebugGiveTestCharacters_Implementation()
{
#if !UE_BUILD_SHIPPING
	ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	if (TDPlayerState == nullptr)
	{
		return;
	}

	// ClassId 는 DT_ClassGrowth 에 실제로 있는 값이어야 성장이 붙는다.
	// 없으면 "Default 성장만 적용된다" 경고가 나오는데, 그것도 정상 동작 확인에 쓸 수 있다.
	TArray<FTDCharacterSummary> Slots;

	FTDCharacterSummary& First = Slots.AddDefaulted_GetRef();
	First.CharacterName = TEXT("테스트A");
	First.ClassId = FName(TEXT("Warrior"));
	First.Level = 25;

	FTDCharacterSummary& Second = Slots.AddDefaulted_GetRef();
	Second.CharacterName = TEXT("테스트B");
	Second.ClassId = FName(TEXT("Mage"));
	Second.Level = 12;

	FTDCharacterSummary& Third = Slots.AddDefaulted_GetRef();
	Third.CharacterName = TEXT("테스트C");
	Third.ClassId = FName(TEXT("Archer"));
	Third.Level = 1;

	TDPlayerState->SetCharacterSlots(MoveTemp(Slots));

	UE_LOG(LogTemp, Log, TEXT("[치트] 테스트 캐릭터 3개를 넣었다. (%s)"), *TDPlayerState->GetPlayerName());
#endif
}
