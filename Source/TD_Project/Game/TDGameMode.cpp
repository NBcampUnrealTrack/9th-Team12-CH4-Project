#include "Game/TDGameMode.h"

#include "AbilitySystemComponent.h"
#include "Abilities/TDAttributeSet.h"
#include "Character/TDCharacterBase.h"
#include "Character/TDPlayerCharacter.h"
#include "Chat/TDChatFilter.h"
#include "Data/TDBannedWordRow.h"
#include "Data/TDZoneEnvironmentRow.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "Game/TDGameState.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Party/TDPartyComponent.h"
#include "Player/TDPlayerController.h"
#include "Player/TDPlayerState.h"
#include "Settings/TDChatSettings.h"
#include "Settings/TDZoneSettings.h"
#include "Stats/TDProgressionComponent.h"

ATDGameMode::ATDGameMode()
{
	// C++ 끼리는 직접 지정한다. DefaultPawnClass 만 BP_GameMode 에서 BP_Player 로 덮어쓴다.
	DefaultPawnClass = ATDPlayerCharacter::StaticClass();
	PlayerControllerClass = ATDPlayerController::StaticClass();
	PlayerStateClass = ATDPlayerState::StaticClass();
	GameStateClass = ATDGameState::StaticClass();

	// 서버가 ServerTravel 로 자기 맵을 통째로 바꿀 때, 접속을 끊지 않고 이어간다.
	// **ServerTravel 에만 걸리는 값이다.** ClientTravel 과는 아무 관계가 없다.
	//
	// 그래서 지금 구조에서 이 값이 실제로 쓰이는 경로는 없다.
	//   - 존 이동     : 같은 월드 안의 좌표 텔레포트라 트래블이 아니다(D47).
	//   - 로그인 : 클라이언트가 자기 로컬 메뉴 레벨에서 ClientTravel 로 접속하는 것이다.
	//                   서버의 맵은 바뀌지 않고 접속 중인 다른 플레이어도 영향을 받지 않는다.
	//   -캐릭터선택: 마지막 맵을 가져와서 해당 존으로 이동시킨다.
	// 그럼에도 true 로 두는 이유는 나중에 누군가 ServerTravel 을 부를 때를 대비한 것이다.
	// false 면 그 순간 접속자 전원이 끊고 다시 붙는다.
	//
	// 주의: true 라도 레벨·인벤토리·장착은 넘어오지 않는다. 트래블에서 살아남는 것은
	// APlayerState::CopyProperties 가 옮기는 값뿐이고, 우리가 추가한 필드는 거기 없다.
	// 이쪽은 세이브에서 다시 읽는 것으로 처리하므로 오버라이드하지 않는다.
	bUseSeamlessTravel = true;

	// 일시정지를 금지한다. 한 사람이 멈추면 서버 전체가 멈춘다 —
	// Pause 는 클라이언트 개인의 화면이 아니라 서버 월드의 시간을 세우는 동작이기 때문이다.
	//
	// 기본값이 true 이므로 반드시 명시적으로 꺼야 한다. AGameModeBase::AllowPausing 은
	//   return bPauseable || GetNetMode() == NM_Standalone;
	// 이라, 이 값이 false 여도 Standalone 에서는 여전히 멈출 수 있다. 엔진 하드코딩이라
	// 막을 수 없지만, 실제 게임은 항상 서버에 붙으므로 문제되지 않는다.
	//
	// ESC 메뉴나 인벤토리를 열 때 게임이 멈추길 기대하면 안 된다는 뜻이기도 하다.
	// UI 는 입력만 가져가고 월드는 계속 돌아간다.
	bPauseable = false;
}

void ATDGameMode::PreLogin(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	// 엔진이 이미 거절했다면 원인을 덮어쓰지 않는다.
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	if (MaxConnectedPlayers > 0 && GetNumPlayers() >= MaxConnectedPlayers)
	{
		ErrorMessage = TEXT("SERVER_FULL");
		UE_LOG(LogTemp, Warning, TEXT("접속 거절 (정원 %d명 초과): %s"), MaxConnectedPlayers, *Address);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("접속 검사 통과: %s"), *Address);
}

void ATDGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	UE_LOG(LogTemp, Log, TEXT("접속 완료: %s (현재 %d명)"),
		*GetNameSafe(NewPlayer), GetNumPlayers());

	// 직업 배정은 캐릭터 선택 화면이 생긴 뒤에 붙인다.
	// 지금은 ClassId 가 비어 있어 DT_ClassGrowth 의 Default 성장만 적용된다.
}

void ATDGameMode::Logout(AController* Exiting)
{
	UE_LOG(LogTemp, Log, TEXT("접속 종료: %s"), *GetNameSafe(Exiting));

	// Super 보다 먼저 처리한다. Super 가 PlayerState 를 PlayerArray 에서 빼버리면
	// 남은 파티원을 찾지 못해 리더 위임과 해체가 일어나지 않는다.
	if (const ATDPlayerState* PlayerState = Exiting ? Exiting->GetPlayerState<ATDPlayerState>() : nullptr)
	{
		if (UTDPartyComponent* Party = PlayerState->GetPartyComponent())
		{
			Party->HandleOwnerLogout();
		}
	}

	Super::Logout(Exiting);
}

// ── 캐릭터 선택  ─────────────────────────────────

void ATDGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	const ATDPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<ATDPlayerState>() : nullptr;

	// 아직 캐릭터를 안 골랐으면 Pawn 을 만들지 않는다. 시점만 선택 공간으로 옮긴다.
	if (PlayerState != nullptr && !PlayerState->HasSelectedCharacter())
	{
		// 별도 카메라 액터를 두지 않고 PlayerStart 의 위치, A방향을 그대로 본다.
		AActor* SelectStart = ChoosePlayerStart(NewPlayer);
		if (SelectStart != nullptr)
		{
			NewPlayer->SetViewTarget(SelectStart);
		}

		UE_LOG(LogTemp, Log,
			TEXT("%s — 캐릭터 미선택. Pawn 을 스폰하지 않고 시점을 '%s' 로 옮긴다."),
			*GetNameSafe(NewPlayer), *GetNameSafe(SelectStart));
		return;
	}

	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void ATDGameMode::HandleCharacterSelected(APlayerController* Player)
{
	if (Player == nullptr)
	{
		return;
	}

	// 선택이 끝났으므로 이제 정상 경로로 스폰한다.
	// ChoosePlayerStart 로 마지막 존의 시작 지점으로 이동한다.
	RestartPlayer(Player);

	// 존을 확정한다. 비워두면 ChoosePlayerStart 가 기본 존으로 폴백해 **스폰만 제대로 되고**,
	// CurrentZoneId 는 None 인 채로 남는다. 그러면 OnZoneChanged 가 한 번도 불리지 않아
	// BGM·라이팅이 초기 상태 그대로이고, 부활 지점 조회도 실패한다.
	//
	// 세이브가 붙으면 그쪽이 LastZoneId 를 먼저 채우므로 이 분기는 신규 캐릭터에만 걸린다.
	if (ATDPlayerState* PlayerState = Player->GetPlayerState<ATDPlayerState>())
	{
		if (!PlayerState->GetCurrentZoneId().IsValid())
		{
			const FGameplayTag DefaultZone =
				FGameplayTag::RequestGameplayTag(DefaultSpawnZoneTag, /*ErrorIfNotFound=*/ false);

			if (DefaultZone.IsValid())
			{
				PlayerState->SetCurrentZoneId(DefaultZone);
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("기본 시작 존 '%s' 가 등록된 게임플레이 태그가 아니다. "
					     "CurrentZoneId 가 비어 있어 BGM·라이팅·부활 지점이 동작하지 않는다."),
					*DefaultSpawnZoneTag.ToString());
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("%s — 캐릭터 선택 완료. Pawn 스폰."), *GetNameSafe(Player));
}

AActor* ATDGameMode::FindPlayerStartByTag(FName Tag) const
{
	if (Tag.IsNone())
	{
		return nullptr;
	}

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag == Tag)
		{
			return *It;
		}
	}

	return nullptr;
}

AActor* ATDGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const ATDPlayerState* PlayerState = Player ? Player->GetPlayerState<ATDPlayerState>() : nullptr;
	if (PlayerState == nullptr)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// 아직 캐릭터를 안 골랐으면 선택 공간으로 보낸다.
	if (!PlayerState->HasSelectedCharacter())
	{
		if (AActor* SelectStart = FindPlayerStartByTag(CharacterSelectStartTag))
		{
			return SelectStart;
		}

		UE_LOG(LogTemp, Warning,
			TEXT("PlayerStartTag 가 '%s' 인 PlayerStart 를 찾지 못했다. "
				 "선택 공간이 아직 배치되지 않았거나, 배치했다면 Is Spatially Loaded 가 켜져 있어 "
				 "언로드된 상태일 수 있다."),
			*CharacterSelectStartTag.ToString());

		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// 캐릭터를 골랐으면 마지막으로 있던 존으로. 신규 캐릭터는 비어 있으므로 기본 시작 존이다.
	// 좌표가 아니라 존만 저장하는 이유는 맵이 수정되면 옛 좌표가 벽 안이 될 수 있어서다.
	const FGameplayTag CurrentZone = PlayerState->GetCurrentZoneId();

	if (AActor* ZoneStart = CurrentZone.IsValid()
		? FindZoneStart(CurrentZone)
		: FindPlayerStartByTag(DefaultSpawnZoneTag))
	{
		return ZoneStart;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("존 '%s' 의 PlayerStart 를 찾지 못해 기본 스폰 지점을 쓴다. "
			 "PlayerStartTag 에 존 태그 문자열을 그대로 넣어야 한다."),
		CurrentZone.IsValid() ? *CurrentZone.ToString() : *DefaultSpawnZoneTag.ToString());

	return Super::ChoosePlayerStart_Implementation(Player);
}

// ══════════════════════════════════════════════════════════════
//  존 이동
//  나중에 컴포넌트로 뗄 때 이 블록을 통째로 옮긴다.
// ══════════════════════════════════════════════════════════════

AActor* ATDGameMode::FindZoneStart(FGameplayTag ZoneId, FName EntryName) const
{
	if (!ZoneId.IsValid())
	{
		return nullptr;
	}

	// 태그 문자열이 곧 PlayerStartTag 다. 매핑 테이블을 두지 않는 이유는
	// 같은 값을 두 곳에 적게 되어 한쪽만 고쳤을 때 어긋나기 때문이다.
	const FString ZoneString = ZoneId.ToString();

	if (!EntryName.IsNone())
	{
		const FName EntryTag = FName(*FString::Printf(TEXT("%s.%s"), *ZoneString, *EntryName.ToString()));

		if (AActor* EntryStart = FindPlayerStartByTag(EntryTag))
		{
			return EntryStart;
		}

		// 못 찾아도 이동 자체는 성공시킨다. 아트가 아직 그 입구를 배치하지 않았을 뿐이고,
		// 여기서 실패시키면 "포탈이 아예 작동하지 않는" 것처럼 보인다.
		UE_LOG(LogTemp, Warning,
			TEXT("진입점 '%s' 를 찾지 못해 기본 진입점으로 보낸다."), *EntryTag.ToString());
	}

	return FindPlayerStartByTag(FName(*ZoneString));
}

const FTDZoneEnvironmentRow* ATDGameMode::FindZoneRow(FGameplayTag ZoneId) const
{
	if (!ZoneId.IsValid())
	{
		return nullptr;
	}

	const UTDZoneSettings* ZoneSettings = GetDefault<UTDZoneSettings>();
	if (ZoneSettings == nullptr)
	{
		return nullptr;
	}

	const UDataTable* Table = ZoneSettings->ZoneEnvironmentTable.LoadSynchronous();
	if (Table == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("존 테이블이 지정되지 않았다. 프로젝트 세팅 > TD > Zone 에서 "
			     "ZoneEnvironmentTable 을 지정할 것."));
		return nullptr;
	}

	// RowName 이 아니라 ZoneId 열로 찾는다. 시트에서 RowName 을 자유롭게 쓰기 위해서다.
	const FTDZoneEnvironmentRow* Found = nullptr;

	Table->ForeachRow<FTDZoneEnvironmentRow>(TEXT("FindZoneRow"),
		[&Found, ZoneId](const FName&, const FTDZoneEnvironmentRow& Row)
		{
			if (Found == nullptr && Row.ZoneId == ZoneId)
			{
				Found = &Row;
			}
		});

	return Found;
}

ETDZoneTravelResult ATDGameMode::RequestZoneTravel(APlayerController* Player, FGameplayTag TargetZoneId,
	FName EntryName, AActor* EntryOverride)
{
	return RequestZoneTravelInternal(Player, TargetZoneId, EntryName, EntryOverride, false);
}

ETDZoneTravelResult ATDGameMode::RequestZoneTravelInternal(APlayerController* Player, FGameplayTag TargetZoneId,
	FName EntryName, AActor* EntryOverride, bool bIsRespawn)
{
	if (!HasAuthority() || Player == nullptr || !TargetZoneId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("존 이동: 대상이나 목적지가 비어 있다."));
		return ETDZoneTravelResult::InternalError;
	}

	ATDPlayerState* PlayerState = Player->GetPlayerState<ATDPlayerState>();
	if (PlayerState == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("존 이동: PlayerState 가 없다."));
		return ETDZoneTravelResult::InternalError;
	}

	// 캐릭터를 고르기 전에는 Pawn 이 없다. 옮길 대상 자체가 없는 상태다.
	APawn* Pawn = Player->GetPawn();
	if (Pawn == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("존 이동: %s 의 Pawn 이 없다. 캐릭터를 먼저 선택해야 한다."),
			*PlayerState->GetPlayerName());
		return ETDZoneTravelResult::InternalError;
	}

	// 테이블에 없는 존이면 거부한다. 클라이언트가 보낸 태그를 그대로 믿지 않는다는 뜻이기도 하다.
	const FTDZoneEnvironmentRow* ZoneRow = FindZoneRow(TargetZoneId);
	if (ZoneRow == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("존 이동 거부: '%s' 가 DT_ZoneEnvironment 에 없다."),
			*TargetZoneId.ToString());
		return ETDZoneTravelResult::ZoneNotFound;
	}

	// 입장 레벨 검사. 클라이언트가 보내는 것은 의도뿐이고 판정은 서버가 한다.
	if (!bIsRespawn && ZoneRow->RequiredLevel > 0)
	{
		const UTDProgressionComponent* Progression = PlayerState->GetProgressionComponent();
		const int32 Level = Progression != nullptr ? Progression->GetLevel() : 1;

		if (Level < ZoneRow->RequiredLevel)
		{
			UE_LOG(LogTemp, Log,
				TEXT("존 이동 거부: '%s' 는 %d 레벨부터 입장할 수 있다. (현재 %d)"),
				*TargetZoneId.ToString(), ZoneRow->RequiredLevel, Level);
			return ETDZoneTravelResult::LevelTooLow;
		}
	}

	// 포탈이 도착 액터를 직접 지정했으면 그것을 쓴다. 문자열을 맞춰 적을 필요가 없어
	// 오타가 생기지 않는다. 지정하지 않았으면 태그로 찾는다.
	AActor* TargetStart = IsValid(EntryOverride)
		? EntryOverride
		: FindZoneStart(TargetZoneId, EntryName);

	if (TargetStart == nullptr)
	{
		// 여기서 옮기면 지형이 없는 허공에 떨어진다. 이동을 취소하는 편이 안전하다(D50).
		UE_LOG(LogTemp, Warning,
			TEXT("존 이동 취소: '%s' 의 PlayerStart 를 찾지 못했다. "
			     "PlayerStartTag 가 존 태그와 같은지, Is Spatially Loaded 가 꺼져 있는지 확인할 것(D60)."),
			*TargetZoneId.ToString());
		return ETDZoneTravelResult::NoEntryPoint;
	}

	const FVector TargetLocation = TargetStart->GetActorLocation();
	const FRotator TargetRotation = TargetStart->GetActorRotation();

	// 떨어지던 속도가 남아 있으면 도착하자마자 바닥으로 파고든다.
	if (ACharacter* TargetCharacter = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Movement = TargetCharacter->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}

	if (!Pawn->TeleportTo(TargetLocation, TargetRotation))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("존 이동 실패: '%s' 로 텔레포트하지 못했다. 도착 지점이 막혀 있을 수 있다."),
			*TargetZoneId.ToString());
		return ETDZoneTravelResult::Blocked;
	}

	// 카메라도 함께 돌려준다. 텔레포트만 하면 시선이 이전 방향을 향한 채로 남는다.
	Player->SetControlRotation(TargetRotation);

	// 위치가 확정된 뒤에 알린다. 먼저 알리면 구독자가 아직 옛 자리에 있는 캐릭터를 본다.
	PlayerState->SetCurrentZoneId(TargetZoneId);

	UE_LOG(LogTemp, Log,
		TEXT("존 이동: %s → '%s'"), *PlayerState->GetPlayerName(), *TargetZoneId.ToString());

	return ETDZoneTravelResult::Success;
}

AActor* ATDGameMode::FindRespawnStart(FGameplayTag ZoneId) const
{
	const FTDZoneEnvironmentRow* Row = FindZoneRow(ZoneId);
	if (Row == nullptr)
	{
		return nullptr;
	}

	return Row->RespawnPlayerStartTag.IsNone()
		? FindZoneStart(ZoneId)
		: FindPlayerStartByTag(Row->RespawnPlayerStartTag);
}

AActor* ATDGameMode::FindNearestRespawnStart(const FVector& Location,
	const TArray<FGameplayTag>& CandidateZoneIds, FGameplayTag& OutZoneId) const
{
	OutZoneId = FGameplayTag();
	AActor* Nearest = nullptr;
	double NearestDistanceSquared = TNumericLimits<double>::Max();
	for (const FGameplayTag& CandidateZoneId : CandidateZoneIds)
	{
		if (AActor* Start = FindRespawnStart(CandidateZoneId))
		{
			const double DistanceSquared = FVector::DistSquared(Location, Start->GetActorLocation());
			if (DistanceSquared < NearestDistanceSquared)
			{
				Nearest = Start;
				NearestDistanceSquared = DistanceSquared;
				OutZoneId = CandidateZoneId;
			}
		}
	}
	return Nearest;
}

ETDZoneTravelResult ATDGameMode::TravelToRespawnZone(APlayerController* Player)
{
	const ATDPlayerState* PlayerState = Player ? Player->GetPlayerState<ATDPlayerState>() : nullptr;
	if (!HasAuthority() || PlayerState == nullptr || Player->GetPawn() == nullptr)
	{
		return ETDZoneTravelResult::InternalError;
	}

	const FTDZoneEnvironmentRow* CurrentRow = FindZoneRow(PlayerState->GetCurrentZoneId());
	if (CurrentRow != nullptr && !CurrentRow->RespawnCandidateZoneIds.IsEmpty())
	{
		FGameplayTag NearestZone;
		if (AActor* Start = FindNearestRespawnStart(Player->GetPawn()->GetActorLocation(),
			CurrentRow->RespawnCandidateZoneIds, NearestZone))
		{
			return RequestZoneTravelInternal(Player, NearestZone, NAME_None, Start, true);
		}
		UE_LOG(LogTemp, Warning, TEXT("부활 후보 지점을 찾지 못해 지정된 부활 존을 사용한다."));
	}

	// 지금 존이 부활 지점을 지정했으면 그쪽, 아니면 기본 시작 존이다.
	FGameplayTag RespawnZone = CurrentRow != nullptr ? CurrentRow->RespawnZoneId : FGameplayTag();
	if (!RespawnZone.IsValid())
	{
		RespawnZone = FGameplayTag::RequestGameplayTag(DefaultSpawnZoneTag, /*ErrorIfNotFound=*/ false);
	}

	if (!RespawnZone.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("부활 이동 실패: 돌아갈 존을 정하지 못했다. "
			     "존의 RespawnZoneId 나 GameMode 의 DefaultSpawnZoneTag 를 확인할 것."));
		return ETDZoneTravelResult::ZoneNotFound;
	}

	AActor* Start = FindRespawnStart(RespawnZone);
	if (Start == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("존 '%s'의 부활 PlayerStart를 찾지 못했다."), *RespawnZone.ToString());
		return ETDZoneTravelResult::NoEntryPoint;
	}
	return RequestZoneTravelInternal(Player, RespawnZone, NAME_None, Start, true);
}

// ══════════════════════════════════════════════════════════════
//  사망·부활
// ══════════════════════════════════════════════════════════════

bool ATDGameMode::RespawnPlayer(APlayerController* Player)
{
	if (!HasAuthority() || Player == nullptr)
	{
		return false;
	}

	ATDCharacterBase* Character = Cast<ATDCharacterBase>(Player->GetPawn());
	if (Character == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("부활 실패: Pawn 이 없다."));
		return false;
	}

	if (!Character->IsDead())
	{
		// 버튼과 타이머가 거의 동시에 도착한 경우다. 먼저 온 쪽이 이미 처리했다.
		return false;
	}

	ATDPlayerState* PlayerState = Player->GetPlayerState<ATDPlayerState>();
	if (PlayerState == nullptr)
	{
		return false;
	}

	// 1. 사망 상태에서 목적지를 확인하고 이동한다. 실패하면 회복과 상태 해제를 하지 않는다.
	const ETDZoneTravelResult TravelResult = TravelToRespawnZone(Player);
	if (TravelResult != ETDZoneTravelResult::Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("부활 이동 실패 (사유 %d). 사망 상태를 유지한다."),
			static_cast<int32>(TravelResult));
		return false;
	}

	// 2. 체력·마나를 절반으로. 패널티가 없는 대신 만피로 살아나지는 않는다.
	if (UAbilitySystemComponent* ASC = PlayerState->GetAbilitySystemComponent())
	{
		const float MaxHealth = ASC->GetNumericAttribute(UTDAttributeSet::GetMaxHealthAttribute());
		const float MaxMana = ASC->GetNumericAttribute(UTDAttributeSet::GetMaxManaAttribute());

		ASC->SetNumericAttributeBase(
			UTDAttributeSet::GetHealthAttribute(), MaxHealth * RespawnVitalRatio);
		ASC->SetNumericAttributeBase(
			UTDAttributeSet::GetManaAttribute(), MaxMana * RespawnVitalRatio);
	}

	// 3. 위치와 생명력이 준비된 뒤 사망 해제 이벤트와 기존 타이머 정리를 실행한다.
	Character->HandleRespawn();

	UE_LOG(LogTemp, Log, TEXT("부활: %s (체력·마나 %.0f%%)"),
		*PlayerState->GetPlayerName(), RespawnVitalRatio * 100.f);

	return true;
}

// ══════════════════════════════════════════════════════════════
//  채팅
// ══════════════════════════════════════════════════════════════

const TArray<FString>& ATDGameMode::GetBannedWords() const
{
	if (bBannedWordsLoaded)
	{
		return CachedBannedWords;
	}

	bBannedWordsLoaded = true;

	const UTDChatSettings* Settings = UTDChatSettings::Get();
	UDataTable* Table = Settings ? Settings->BannedWordTable.LoadSynchronous() : nullptr;

	if (Table == nullptr)
	{
		// 지정하지 않은 것도 선택이다. 경고만 남기고 아무것도 거르지 않는다 —
		// 여기서 막아버리면 필터를 붙이기 전까지 채팅 자체를 못 쓴다.
		UE_LOG(LogTemp, Warning,
			TEXT("채팅 금지어 테이블이 지정되지 않았다. 프로젝트 세팅 > TD > Chat 을 확인할 것."));
		return CachedBannedWords;
	}

	Table->ForeachRow<FTDBannedWordRow>(TEXT("GetBannedWords"),
		[this](const FName&, const FTDBannedWordRow& Row)
		{
			if (!Row.Word.IsEmpty())
			{
				CachedBannedWords.Add(Row.Word);
			}
		});

	UE_LOG(LogTemp, Log, TEXT("채팅 금지어 %d개를 읽었다."), CachedBannedWords.Num());

	return CachedBannedWords;
}

void ATDGameMode::DeliverChat(APlayerController* Target, ETDChatChannel Channel,
	const FString& SenderName, const FString& Message) const
{
	if (ATDPlayerController* TDController = Cast<ATDPlayerController>(Target))
	{
		TDController->ClientReceiveChat(Channel, SenderName, Message);
	}
}

APlayerController* ATDGameMode::FindPlayerControllerByName(const FString& PlayerName) const
{
	const AGameStateBase* State = GameState;
	if (State == nullptr || PlayerName.IsEmpty())
	{
		return nullptr;
	}

	for (APlayerState* PlayerState : State->PlayerArray)
	{
		if (PlayerState == nullptr)
		{
			continue;
		}

		// 대소문자를 구분하지 않는다. 귓속말은 손으로 이름을 치는 경우가 대부분이라
		// 정확히 맞춰 적기를 요구하면 안 그래도 불편한 것이 더 불편해진다.
		if (PlayerState->GetPlayerName().Equals(PlayerName, ESearchCase::IgnoreCase))
		{
			return PlayerState->GetPlayerController();
		}
	}

	return nullptr;
}

ETDChatSendResult ATDGameMode::RouteChatMessage(APlayerController* Sender, ETDChatChannel Channel,
	const FString& Message, const FString& TargetName)
{
	// 앞뒤 공백은 버린다. 공백만 보내는 것으로 화면을 밀어 올릴 수 있기 때문이다.
	const FString Trimmed = Message.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return ETDChatSendResult::Empty;
	}

	const UTDChatSettings* Settings = UTDChatSettings::Get();
	if (Settings != nullptr && Trimmed.Len() > Settings->MaxMessageLength)
	{
		// 잘라내지 않고 거부한다. 잘리면 뒷말이 사라진 줄 모른 채 대화가 이어진다.
		return ETDChatSendResult::TooLong;
	}

	// System 과 Loot 은 서버만 쓴다. Sender 가 있다는 것은 플레이어 요청이라는 뜻이다.
	const bool bServerOnlyChannel = Channel == ETDChatChannel::System || Channel == ETDChatChannel::Loot;
	if (Sender != nullptr && bServerOnlyChannel)
	{
		UE_LOG(LogTemp, Warning, TEXT("채팅: %s 가 서버 전용 채널로 보내려 했다."),
			Sender->PlayerState ? *Sender->PlayerState->GetPlayerName() : TEXT("알 수 없음"));
		return ETDChatSendResult::ChannelNotAllowed;
	}

	// 검열은 플레이어가 쓴 것에만 건다. 서버가 만든 문장을 자기가 다시 거를 이유가 없다.
	FString FinalMessage = Trimmed;
	if (Sender != nullptr)
	{
		bool bMasked = false;
		FinalMessage = TDChatFilter::Mask(Trimmed, GetBannedWords(), bMasked);

		if (bMasked)
		{
			UE_LOG(LogTemp, Log, TEXT("채팅 필터: %s 의 메시지를 가렸다."),
				Sender->PlayerState ? *Sender->PlayerState->GetPlayerName() : TEXT("알 수 없음"));
		}
	}

	const FString SenderName = (Sender != nullptr && Sender->PlayerState != nullptr)
		? Sender->PlayerState->GetPlayerName()
		: FString();

	switch (Channel)
	{
	case ETDChatChannel::All:
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			DeliverChat(It->Get(), Channel, SenderName, FinalMessage);
		}
		return ETDChatSendResult::Success;

	case ETDChatChannel::Party:
	{
		const ATDPlayerState* SenderState = Sender ? Sender->GetPlayerState<ATDPlayerState>() : nullptr;
		UTDPartyComponent* Party = SenderState ? SenderState->GetPartyComponent() : nullptr;

		if (Party == nullptr || !Party->IsInParty())
		{
			return ETDChatSendResult::NotInParty;
		}

		for (ATDPlayerState* Member : Party->GetPartyMembers())
		{
			if (Member != nullptr)
			{
				DeliverChat(Member->GetPlayerController(), Channel, SenderName, FinalMessage);
			}
		}
		return ETDChatSendResult::Success;
	}

	case ETDChatChannel::Whisper:
	{
		APlayerController* Target = FindPlayerControllerByName(TargetName);
		if (Target == nullptr)
		{
			return ETDChatSendResult::TargetNotFound;
		}

		DeliverChat(Target, Channel, SenderName, FinalMessage);

		// 보낸 사람도 자기 화면에서 확인해야 한다. 자기에게 귓속말한 경우
		// 두 번 뜨지 않도록 대상과 같으면 건너뛴다.
		if (Sender != Target)
		{
			DeliverChat(Sender, Channel, SenderName, FinalMessage);
		}
		return ETDChatSendResult::Success;
	}

	case ETDChatChannel::System:
	case ETDChatChannel::Loot:
		// 여기까지 온 것은 서버가 부른 경우다. 대상을 정하는 것은 부른 쪽의 몫이라
		// SendSystemMessage / BroadcastSystemMessage 를 쓴다.
		UE_LOG(LogTemp, Warning,
			TEXT("채팅: 서버 전용 채널은 SendSystemMessage 를 쓸 것."));
		return ETDChatSendResult::ChannelNotAllowed;
	}

	return ETDChatSendResult::Success;
}

void ATDGameMode::SendSystemMessage(APlayerController* Target, ETDChatChannel Channel, const FString& Message)
{
	if (Target == nullptr || Message.IsEmpty())
	{
		return;
	}

	// 보낸 사람 이름이 비어 있다. UI 는 이 채널에서 이름을 그리지 않으면 된다.
	DeliverChat(Target, Channel, FString(), Message);
}

void ATDGameMode::BroadcastSystemMessage(const FString& Message)
{
	if (Message.IsEmpty())
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		DeliverChat(It->Get(), ETDChatChannel::System, FString(), Message);
	}
}
