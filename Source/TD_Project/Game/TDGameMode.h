#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "TDGameMode.generated.h"

struct FTDZoneEnvironmentRow;

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
	 * @return 실제로 옮겼으면 true.
	 */
	bool RequestZoneTravel(APlayerController* Player, FGameplayTag TargetZoneId,
		FName EntryName = NAME_None, AActor* EntryOverride = nullptr);

	/**
	 * 사망 후 부활 지점으로 보낸다. 지금 존의 RespawnZoneId 를 따르며,
	 * 지정돼 있지 않으면 기본 시작 존으로 간다.
	 *
	 * 부활 자체(체력 회복·상태 해제)는 하지 않는다. 위치만 옮긴다.
	 */
	bool TravelToRespawnZone(APlayerController* Player);

	/** 존 정의를 읽는다. 없으면 nullptr. UI·전투가 규칙을 물을 때도 쓴다. */
	const FTDZoneEnvironmentRow* FindZoneRow(FGameplayTag ZoneId) const;

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

private:
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
};
