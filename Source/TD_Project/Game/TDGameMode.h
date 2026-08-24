#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TDGameMode.generated.h"

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
	 * 동시 접속 상한. 30명 이하 규모에서는 언리얼 기본 복제로 충분하므로
	 * 이 값을 크게 올리려면 복제 최적화를 먼저 검토해야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Network", meta = (ClampMin = "1"))
	int32 MaxConnectedPlayers = 30;
};
