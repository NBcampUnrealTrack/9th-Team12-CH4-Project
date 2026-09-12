#pragma once

#include "CoreMinimal.h"
#include "Chat/TDChatTypes.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "TDGameMode.generated.h"

struct FTDZoneEnvironmentRow;

/**
 * 존 이동 요청의 결과. 실패 사유를 클라이언트에 돌려주기 위한 값이다.
 *
 * FText 메시지가 아니라 enum 인 이유는 **문구를 UI 가 정해야** 하기 때문이다.
 * 서버가 "24레벨부터 입장 가능합니다" 를 문자열로 만들어 보내면 현지화도 못 하고
 * 화면 디자인이 서버 코드에 묶인다.
 */
UENUM(BlueprintType)
enum class ETDZoneTravelResult : uint8
{
	Success			UMETA(DisplayName = "성공"),

	/** 입장 레벨이 모자란다. UI 는 존의 RequiredLevel 을 함께 보여주면 된다. */
	LevelTooLow		UMETA(DisplayName = "레벨 부족"),

	/** DT_ZoneEnvironment 에 그 존이 없다. 데이터 누락이거나 조작된 요청이다. */
	ZoneNotFound	UMETA(DisplayName = "존 없음"),

	/** 도착 지점을 찾지 못했다. PlayerStart 배치 문제라 플레이어 잘못이 아니다. */
	NoEntryPoint	UMETA(DisplayName = "도착 지점 없음"),

	/** 도착 지점이 막혀 텔레포트가 실패했다. 다른 플레이어가 서 있는 경우 등. */
	Blocked			UMETA(DisplayName = "도착 지점 막힘"),

	/** Pawn 이 없거나 권한이 없다. 정상 흐름에서는 나오지 않는다. */
	InternalError	UMETA(DisplayName = "내부 오류")
};

/**
 * 서버의 게임 규칙.
 *
 * 데디케이티드 서버에만 존재하며 클라이언트에는 생성되지 않는다.
 * 클라이언트에서 GetGameMode() 를 부르면 nullptr 이므로, UI 가 읽어야 하는 것은
 * 여기가 아니라 ATDGameState 에 둔다.
 *
 * 클래스 지정만 C++ 로 하고 DefaultPawnClass 는 블루프린트에서 덮어쓴다.
 * BP_Player 에 카메라·메시 설정이 들어가는데, C++ 에서 블루프린트를 참조하면
 * 에셋 경로가 코드에 박혀 파일을 옮길 때마다 깨지기 때문이다.
 *
 * ── 나중에 컴포넌트로 쪼갤 때 ──
 * 관심사가 셋 이상 섞이고 400줄을 넘어가면 나눈다. 그때 **먼저 물을 것은
 * "GameMode 냐 GameState 냐"** 다.
 *
 *   GameMode 컴포넌트    서버에만 존재. 순수 서버 판정만 담을 수 있다
 *   GameState 컴포넌트   복제된다. 모두가 알아야 하는 상태 (필드 보스, 월드 이벤트)
 *
 * 습관적으로 GameMode 컴포넌트를 만들면 나중에 "클라에서 이 값을 못 읽네" 하고
 * GameState 로 다시 옮기게 된다. 존 이동은 순수 서버 판정이라 이쪽이 맞다 —
 * 클라이언트는 결과(`CurrentZoneId`)만 복제로 받는다.
 *
 * 엔진 오버라이드(PreLogin·ChoosePlayerStart 등)는 가상 함수라 떼어낼 수 없다.
 * 아래 구획으로 묶어둔 블록만 옮기면 된다.
 */
UCLASS()
class TD_PROJECT_API ATDGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATDGameMode();

	virtual void PreLogin(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	virtual void PostLogin(APlayerController* NewPlayer) override;

	virtual void Logout(AController* Exiting) override;

	/**
	 * 캐릭터 선택이 끝났을 때 PlayerState 가 부른다. 여기서 비로소 Pawn 을 만든다.
	 *
	 * 스폰 권한은 GameMode 에만 있다. PlayerState 가 직접 Pawn 을 만들면
	 * 스폰 지점 결정·재접속·리스폰 경로가 두 군데로 갈라진다.
	 */
	void HandleCharacterSelected(APlayerController* Player);

	// ══════════════════════════════════════════════════════════
	//  존 이동
	//  나중에 컴포넌트로 뗄 때 이 블록을 통째로 옮긴다.
	// ══════════════════════════════════════════════════════════

	/**
	 * 플레이어를 다른 존으로 옮긴다. **서버 전용이며 요청을 거부할 수 있다.**
	 *
	 * 포탈은 이 함수만 부르면 된다. 목적지의 좌표도, BGM 도, 라이팅도 몰라야 한다 —
	 *
	 * 거부하는 경우: 권한 없음 / 테이블에 없는 존 / 레벨 부족 / PlayerStart 없음.
	 * 전부 로그를 남긴다 — 조용히 실패하면 원인을 찾을 수 없다.
	 *
	 * 도착 지점은 셋 중 먼저 찾아지는 것으로 정해진다.
	 *
	 *   1. EntryOverride    포탈이 액터를 직접 지정한 경우 — 오타가 불가능해 가장 안전하다
	 *   2. EntryName        "Zone.Region1.Field02.West" 형태의 PlayerStartTag
	 *   3. 기본 진입점       존 태그 그대로. 로그인 복귀·부활이 쓰는 경로다
	 *
	 * @param EntryName     존 안의 어느 입구로 나올지. 비우면 기본 진입점.
	 *                      한 존에 입구가 여럿일 때(사냥터2 를 1 에서도 3 에서도 들어감)
	 *                      **어느 문으로 들어왔는지는 문이 아는 정보**라 포탈이 정한다.
	 * @param EntryOverride 도착 액터를 직접 넘긴다. 월드가 하나뿐이라 포탈이
	 *                      에디터에서 도착 지점을 드래그로 지정할 수 있고, 그러면
	 *                      문자열을 두 곳에 맞춰 적을 필요가 없어진다.
	 *
	 *                      그래도 이 함수를 거쳐야 한다 — 포탈이 직접 TeleportTo 를 부르면
	 *                      레벨 검증·ZoneId 갱신·속도 초기화가 전부 빠진다.
	 * @return 성공 여부와 실패 사유. 호출한 쪽이 클라이언트에 알려줄 수 있도록
	 *         bool 이 아니라 사유를 돌려준다 — 거부됐는데 화면에 아무 일도 일어나지 않으면
	 *         플레이어는 "포탈이 고장났다" 고 받아들인다.
	 */
	ETDZoneTravelResult RequestZoneTravel(APlayerController* Player, FGameplayTag TargetZoneId,
		FName EntryName = NAME_None, AActor* EntryOverride = nullptr);

	/**
	 * 사망 후 부활 지점으로 보낸다. 후보가 있으면 가장 가까운 지점을,
	 * 없으면 RespawnZoneId 를 따른다. 지정돼 있지 않으면 기본 시작 존으로 간다.
	 *
	 * 부활 자체(체력 회복·상태 해제)는 하지 않는다. 위치만 옮긴다.
	 */
	ETDZoneTravelResult TravelToRespawnZone(APlayerController* Player);

	/** 존 정의를 읽는다. 없으면 nullptr. UI·전투가 규칙을 물을 때도 쓴다. */
	const FTDZoneEnvironmentRow* FindZoneRow(FGameplayTag ZoneId) const;

	// ══════════════════════════════════════════════════════════
	//  사망·부활
	// ══════════════════════════════════════════════════════════

	/**
	 * 되살린다. **서버 전용.** 부활 버튼과 자동 부활 타이머가 같은 이 함수를 부른다.
	 *
	 * 두 경로가 갈라지면 한쪽만 고쳤을 때 "버튼으로는 되는데 타이머로는 안 되는"
	 * 상태가 된다. 어느 쪽이 먼저 오든 여기서 한 번만 처리되고,
	 * 남은 타이머는 캐릭터의 HandleRespawn 이 지운다.
	 *
	 * 하는 일:
	 *   1. 부활 목적지로 이동 (실패하면 사망 상태 유지)
	 *   2. 체력·마나를 절반으로 회복
	 *   3. 사망 상태 해제 (조작 복구 및 자동 타이머 정리)
	 *
	 * @return 실제로 되살렸으면 true. 이동 실패 또는 유효한 사망 캐릭터가 없으면 false.
	 */
	bool RespawnPlayer(APlayerController* Player);

	/** 자동 부활까지의 시간(초). 0 이면 자동 부활 없이 버튼으로만 되살아난다. */
	UFUNCTION(BlueprintPure, Category = "TD|Combat")
	float GetAutoRespawnSeconds() const { return AutoRespawnSeconds; }

	// ══════════════════════════════════════════════════════════
	//  채팅
	// ══════════════════════════════════════════════════════════

	/**
	 * 채팅을 검사하고 채널에 맞는 사람들에게 보낸다. **서버 전용.**
	 *
	 * 라우팅이 GameMode 에 있는 이유는 접속자 전원을 아는 자리가 여기뿐이기 때문이다.
	 * PlayerController 는 자기 자신만 알고, PlayerState 는 남의 것을 뒤질 이유가 없다.
	 *
	 * 검사 순서는 싼 것부터다 — 빈 내용·길이·채널 권한을 먼저 보고, 금지어 필터는
	 * 통과한 메시지에만 건다. 필터가 목록 전체를 훑으므로 가장 비싸다.
	 *
	 * @param Sender      보낸 사람. 시스템 메시지는 nullptr 이다.
	 * @param TargetName  귓속말 대상의 PlayerName. 다른 채널에서는 무시한다.
	 * @return 성공 여부와 거부 사유. 호출한 쪽이 보낸 사람에게 알려준다.
	 */
	ETDChatSendResult RouteChatMessage(APlayerController* Sender, ETDChatChannel Channel,
		const FString& Message, const FString& TargetName = FString());

	/**
	 * 시스템 메시지를 한 명에게 보낸다. 획득 알림(Loot)도 이 함수를 쓴다.
	 *
	 * 전투·인벤토리 쪽에서 "경험치 120 획득" 같은 것을 띄울 때 부르면 된다.
	 * 검열과 쿨다운을 거치지 않는다 — 서버가 만든 문장이기 때문이다.
	 *
	 * 블루프린트에도 연다. 이벤트나 연출을 BP 로 만드는 쪽에서 "보스가 등장했습니다"
	 * 같은 것을 띄우려면 필요하다. 채널은 System 이나 Loot 만 의미가 있다 —
	 * 대화 채널을 넣으면 보낸 사람 이름 없이 나가서 UI 가 어색하게 그린다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TD|Chat", meta = (BlueprintAuthorityOnly = "true"))
	void SendSystemMessage(APlayerController* Target, ETDChatChannel Channel, const FString& Message);

	/** 접속자 전원에게 공지를 보낸다. 점검 안내 같은 것. */
	UFUNCTION(BlueprintCallable, Category = "TD|Chat", meta = (BlueprintAuthorityOnly = "true"))
	void BroadcastSystemMessage(const FString& Message);

protected:
	/**
	 * 캐릭터를 고르기 전에는 Pawn 을 만들지 않는다.
	 *
	 * 기본 구현은 접속 즉시 Pawn 을 스폰하는데, 그러면 선택 화면에서 캐릭터를 조종할 수 있게 되고
	 * 그걸 막는 코드를 따로 짜야 한다. 아예 만들지 않는 편이 간단하다.
	 */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/**
	 * 스폰 지점을 고른다.
	 *
	 * 아직 캐릭터를 안 골랐으면 CharacterSelectStartTag 가 붙은 PlayerStart 로,
	 * 골랐으면 그 캐릭터의 마지막 존으로 보낸다. 좌표를 저장하지 않는 이유는
	 * 맵이 수정되면 벽 안이나 허공에 떨어질 수 있기 때문이다.
	 */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/**
	 * 캐릭터 선택 공간의 PlayerStart 에 붙일 태그.
	 *
	 * 좌표를 코드에 박지 않는 이유는 그 자리가 아직 정해지지 않았고,
	 * 정해진 뒤에도 옮길 수 있기 때문이다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Character")
	FName CharacterSelectStartTag = TEXT("CharacterSelect");

	/**
	 * 신규 캐릭터가 시작할 존. LastZoneId 가 비어 있을 때 쓴다.
	 *
	 * 존별 PlayerStart 의 태그는 존 태그 문자열을 그대로 쓴다(예: "Zone.Region1.Town").
	 * 따로 매핑 테이블을 두지 않아도 FGameplayTag::ToString() 으로 바로 비교된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Character")
	FName DefaultSpawnZoneTag = TEXT("Zone.Region1.Town");

	/**
	 * 죽고 나서 자동으로 되살아나기까지의 시간(초).
	 *
	 * 부활 버튼과 함께 둔다. 버튼만 있으면 자리를 비운 사이 시체로 남고,
	 * 타이머만 있으면 기다리기 싫은 사람이 접속을 끊었다 다시 들어온다.
	 *
	 * 0 으로 두면 자동 부활을 쓰지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Combat", meta = (ClampMin = "0.0"))
	float AutoRespawnSeconds = 10.f;

	/** 부활 시 회복되는 체력·마나 비율. 패널티 대신 절반으로 시작한다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Combat", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float RespawnVitalRatio = 0.5f;

private:
	/** 포탈과 부활의 이동 처리를 공유한다. 입장 레벨 면제는 서버의 부활 경로만 사용한다. */
	ETDZoneTravelResult RequestZoneTravelInternal(APlayerController* Player, FGameplayTag TargetZoneId,
		FName EntryName, AActor* EntryOverride, bool bIsRespawn);

	/** 목적지 행에 설정한 부활 PlayerStartTag, 또는 기존 ZoneId 기본 지점. */
	AActor* FindRespawnStart(FGameplayTag ZoneId) const;

	/** 후보 목록의 순서는 같은 거리일 때의 우선순위다. 로드되지 않은 지점은 건너뛴다. */
	AActor* FindNearestRespawnStart(const FVector& Location, const TArray<FGameplayTag>& CandidateZoneIds,
		FGameplayTag& OutZoneId) const;


	/**
	 * 태그가 일치하는 PlayerStart 를 찾는다. 없으면 nullptr.
	 *
	 * 주의: PlayerStart 는 World Partition 셀에 속하므로 Is Spatially Loaded 를 꺼야 한다.
	 * 언로드된 액터는 여기서 보이지 않아 조용히 기본 스폰으로 넘어간다.
	 */
	AActor* FindPlayerStartByTag(FName Tag) const;

	/**
	 * 존 태그로 PlayerStart 를 찾는다. 태그 문자열을 그대로 쓴다(D59).
	 *
	 * 존 이동과 스폰이 같은 규칙을 쓰게 하려고 한 곳으로 모았다. 갈라놓으면
	 * 한쪽만 고쳤을 때 "접속하면 맞는 자리인데 포탈로 가면 엉뚱한 자리" 가 된다.
	 *
	 * ── 진입점이 여럿일 때 ──
	 * PlayerStartTag 에 접미사를 붙여 구분한다. 진입점을 게임플레이 태그로 만들지
	 * 않는 이유는 존 15개 × 입구 2~3개만으로도 태그가 폭발하기 때문이다.
	 *
	 *   Zone.Region1.Field02          기본 진입점 — 로그인 복귀·부활이 쓴다
	 *   Zone.Region1.Field02.West     서쪽 입구
	 *   Zone.Region1.Field02.East     동쪽 입구
	 *
	 * EntryName 을 찾지 못하면 **기본 진입점으로 떨어진다.** 아트가 아직 입구를
	 * 배치하지 않았어도 이동은 성공해야 하기 때문이다. 대신 경고를 남긴다.
	 */
	AActor* FindZoneStart(FGameplayTag ZoneId, FName EntryName = NAME_None) const;

	/**
	 * 동시 접속 상한. 30명 이하 규모에서는 언리얼 기본 복제로 충분하므로
	 * 이 값을 크게 올리려면 복제 최적화를 먼저 검토해야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Network", meta = (ClampMin = "1"))
	int32 MaxConnectedPlayers = 30;

	// ── 채팅 ──────────────────────────────────────────────

	/** 한 명에게 실제로 보낸다. 채널별 대상 추리기가 끝난 뒤 불린다. */
	void DeliverChat(APlayerController* Target, ETDChatChannel Channel,
		const FString& SenderName, const FString& Message) const;

	/** 접속자 중 이 이름을 가진 사람을 찾는다. 귓속말 대상 확인용. */
	APlayerController* FindPlayerControllerByName(const FString& PlayerName) const;

	/**
	 * 금지어 목록. 테이블을 매번 훑지 않으려고 처음 한 번만 읽어 둔다.
	 *
	 * 목록을 고쳤을 때 서버를 다시 띄워야 반영된다. 운영 중에 바꿀 일이 잦아지면
	 * 그때 갱신 명령을 두면 된다 — 지금 넣으면 쓰지도 않는 코드가 된다.
	 */
	const TArray<FString>& GetBannedWords() const;

	mutable TArray<FString> CachedBannedWords;
	mutable bool bBannedWordsLoaded = false;
};
