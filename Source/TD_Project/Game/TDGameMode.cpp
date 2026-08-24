#include "Game/TDGameMode.h"

#include "Character/TDPlayerCharacter.h"
#include "EngineUtils.h"
#include "Game/TDGameState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Player/TDPlayerController.h"
#include "Player/TDPlayerState.h"

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
	// 좌표가 아니라 존만 저장하는 이유는 맵이 수정되면 옛 좌표가 벽 안이 될 수 있어서다(D51).
	const FGameplayTag LastZone = PlayerState->GetLastZoneId();
	const FName ZoneTag = LastZone.IsValid() ? FName(*LastZone.ToString()) : DefaultSpawnZoneTag;

	if (AActor* ZoneStart = FindPlayerStartByTag(ZoneTag))
	{
		return ZoneStart;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("존 '%s' 의 PlayerStart 를 찾지 못해 기본 스폰 지점을 쓴다. "
			 "PlayerStartTag 에 존 태그 문자열을 그대로 넣어야 한다."),
		*ZoneTag.ToString());

	return Super::ChoosePlayerStart_Implementation(Player);
}
