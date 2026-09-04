#include "Player/TDPlayerController.h"

#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Character/TDCharacterBase.h"
#include "Combat/TDCombatStatics.h"
#include "Core/TDGameInstance.h"
#include "Game/TDGameMode.h"
#include "GameFramework/PlayerState.h"
#include "Items/TDInventoryComponent.h"
#include "Party/TDPartyComponent.h"
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

void ATDPlayerController::ServerDebugGiveGold_Implementation(int32 Amount)
{
#if !UE_BUILD_SHIPPING
	if (PlayerState == nullptr)
	{
		return;
	}

	if (UTDInventoryComponent* Inventory = PlayerState->FindComponentByClass<UTDInventoryComponent>())
	{
		const bool bAdded = Inventory->AddGold(Amount);
		UE_LOG(LogTemp, Log, TEXT("[치트] 골드 %d 지급 %s (보유 %d)"),
			Amount, bAdded ? TEXT("성공") : TEXT("실패"), Inventory->GetGold());
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

void ATDPlayerController::ServerDebugAddExp_Implementation(int32 Amount)
{
#if !UE_BUILD_SHIPPING
	if (PlayerState == nullptr)
	{
		return;
	}

	UTDProgressionComponent* Progression = PlayerState->FindComponentByClass<UTDProgressionComponent>();
	if (Progression == nullptr)
	{
		return;
	}

	Progression->AddExp(Amount);

	UE_LOG(LogTemp, Log, TEXT("[치트] 경험치 %d 지급. Level %d, 누적 %d, 다음까지 %d (%.0f%%)"),
		Amount,
		Progression->GetLevel(),
		Progression->GetExp(),
		Progression->GetExpToNextLevel(),
		Progression->GetLevelProgress() * 100.f);
#endif
}

void ATDPlayerController::ServerDebugDamage_Implementation(float Amount)
{
#if !UE_BUILD_SHIPPING
	// AController 에 이미 Character 멤버가 있어 그 이름은 쓸 수 없다(C4458).
	ATDCharacterBase* TargetCharacter = Cast<ATDCharacterBase>(GetPawn());
	if (TargetCharacter == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[치트] 조종 중인 캐릭터가 없다. TD.SelectCharacter 로 먼저 스폰할 것."));
		return;
	}

	UTDCombatStatics::ApplyRawDamage(TargetCharacter, Amount);

	// 결과를 함께 찍는다. 요청만 보내고 끝나면 실제로 깎였는지 알 수 없다.
	if (const UAbilitySystemComponent* ASC = TargetCharacter->GetAbilitySystemComponent())
	{
		UE_LOG(LogTemp, Log, TEXT("[치트] %.0f 피해. 체력 %.1f / %.1f%s"),
			Amount,
			ASC->GetNumericAttribute(UTDAttributeSet::GetHealthAttribute()),
			ASC->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute()),
			TargetCharacter->IsDead() ? TEXT(" [사망]") : TEXT(""));
	}
#endif
}

void ATDPlayerController::ServerDebugTravelToZone_Implementation(FGameplayTag TargetZoneId, FName EntryName)
{
#if !UE_BUILD_SHIPPING
	ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr;
	if (GameMode == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[치트] GameMode 를 찾지 못했다."));
		return;
	}

	// 거부 사유는 RequestZoneTravel 이 직접 로그로 남긴다. 여기서는 결과만 요약한다.
	const ETDZoneTravelResult Result = GameMode->RequestZoneTravel(this, TargetZoneId, EntryName);

	UE_LOG(LogTemp, Log, TEXT("[치트] 존 이동 %s — '%s' (사유 %d)"),
		Result == ETDZoneTravelResult::Success ? TEXT("성공") : TEXT("거부됨"),
		*TargetZoneId.ToString(), static_cast<int32>(Result));
#endif
}

void ATDPlayerController::ClientZoneTravelFailed_Implementation(FGameplayTag TargetZoneId,
	ETDZoneTravelResult Reason)
{
	// 문구는 만들지 않는다. UI 가 이 델리게이트를 받아 자기 형식으로 표시한다.
	OnZoneTravelFailed.Broadcast(TargetZoneId, Reason);

	// UI 가 붙기 전까지는 로그로만 확인한다.
	UE_LOG(LogTemp, Log, TEXT("존 이동 거부됨: '%s' (사유 %d)"),
		*TargetZoneId.ToString(), static_cast<int32>(Reason));
}

void ATDPlayerController::ServerDebugPartyExp_Implementation(int32 BaseAmount)
{
#if !UE_BUILD_SHIPPING
	const ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	UTDPartyComponent* Party = TDPlayerState ? TDPlayerState->GetPartyComponent() : nullptr;

	if (Party == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[치트] PartyComponent 를 찾지 못했다."));
		return;
	}

	const int32 Awarded = Party->AwardKillExp(BaseAmount);

	UE_LOG(LogTemp, Log,
		TEXT("[치트] 처치 경험치 %d 분배 → %d명이 받았다 (보너스 +%.0f%%, 존 '%s' 기준)"),
		BaseAmount, Awarded, Party->GetExpBonusRate() * 100.f,
		*TDPlayerState->GetCurrentZoneId().ToString());
#endif
}

void ATDPlayerController::ServerRequestRespawn_Implementation()
{
	// 치트가 아니므로 Shipping 가드를 두지 않는다.
	if (ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr)
	{
		GameMode->RespawnPlayer(this);
	}
}

void ATDPlayerController::ServerDebugQuickStart_Implementation(int32 SlotIndex)
{
#if !UE_BUILD_SHIPPING
	ATDPlayerState* TDPlayerState = GetPlayerState<ATDPlayerState>();
	if (TDPlayerState == nullptr)
	{
		return;
	}

	// 지급과 선택이 같은 함수 안에서 순서대로 일어난다. 서버에서 직접 부르므로
	// 두 RPC 사이에 다른 것이 끼어들 여지가 없다.
	ServerDebugGiveTestCharacters_Implementation();

	const bool bSelected = TDPlayerState->SelectCharacter(SlotIndex);

	UE_LOG(LogTemp, Log, TEXT("[치트] 빠른 시작: %d번 캐릭터 선택 %s"),
		SlotIndex, bSelected ? TEXT("성공") : TEXT("실패"));
#endif
}
