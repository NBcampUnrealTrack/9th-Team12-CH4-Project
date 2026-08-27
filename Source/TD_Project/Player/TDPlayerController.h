#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TDPlayerController.generated.h"

/**
 * 플레이어 한 명의 의도를 나타내는 Controller.
 *
 * 서버와 그 플레이어의 클라이언트에만 존재한다. 다른 플레이어의 컨트롤러는 보이지 않는다.
 * 소유 관계가 명확해서 Server RPC 를 받기에 가장 안전한 자리이기도 하다.
 *
 * 지금은 접속 명령만 있다. Enhanced Input 바인딩과 CheatManager 가 나중에 여기 붙는다 —
 * 콘솔 명령(TD.*)도 GameMode 가 자리를 잡으면 CheatManager 로 옮길 수 있다.
 *
 * 이동 입력은 여기서 다루지 않는다. CharacterMovementComponent 가 예측과 서버 재현을
 * 이미 처리하므로 캐릭터 쪽에서 AddMovementInput 만 부르면 된다.
 */
UCLASS()
class TD_PROJECT_API ATDPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/**
	 * 개발용 접속 명령. 콘솔에 직접 입력한다.
	 *
	 *   TDConnect 127.0.0.1:7777
	 *   TDConnect 25.10.20.30:7777
	 */
	UFUNCTION(Exec)
	void TDConnect(const FString& Address);

	/**
	 * 이 플레이어 한 명만 다른 서버로 보낸다.
	 *
	 * 아직 호출하는 곳이 없다. 인스턴스 던전을 별도 서버로 띄우게 되면 그때 쓴다.
	 * ServerTravel 과 달리 접속자 전원이 아니라 대상 한 명만 이동한다.
	 */
	UFUNCTION(Client, Reliable)
	void ClientTravelToServer(const FString& Address);

	// ── 개발용 치트 ───────────────────────────────────────
	// 클라이언트에서 콘솔 명령을 쳤을 때 서버까지 전달하기 위한 통로다.
	//
	// 인벤토리 지급과 레벨 변경은 정상 경로에 RPC 를 두지 않았다 —
	// 열어두면 클라이언트가 스스로 아이템을 만들고 레벨을 올릴 수 있기 때문이다.
	//
	// UFUNCTION 은 전처리기 블록 안에 둘 수 없어서 선언은 항상 남는다.
	// 구현부를 UE_BUILD_SHIPPING 으로 막아 배포 빌드에서는 아무 일도 하지 않게 했다.
	// 선언까지 없애려면 CheatManager 로 옮겨야 한다 — 그쪽은 Shipping 에서 객체 자체가
	// 만들어지지 않으므로 더 확실하다. GameMode 가 자리를 잡았으니 나중에 이관할 것.

	UFUNCTION(Server, Reliable)
	void ServerDebugGiveItem(FName ItemId, int32 Count);

	UFUNCTION(Server, Reliable)
	void ServerDebugSetLevel(int32 NewLevel);

	UFUNCTION(Server, Reliable)
	void ServerDebugSetClass(FName NewClassId);

	/**
	 * 캐릭터 목록에 더미를 채운다. 세이브 담당이 붙으면 필요 없어진다.
	 *
	 * 목록을 채우는 것은 서버 권한인데 Play As Client 로 띄우면 서버 콘솔이 없으므로,
	 * 클라이언트 창에서도 테스트할 수 있게 통로를 연다.
	 */
	UFUNCTION(Server, Reliable)
	void ServerDebugGiveTestCharacters();

	/**
	 * 자기 캐릭터에게 고정 피해를 적용한다.
	 *
	 * ApplyRawDamage 는 서버 권한을 요구하므로 클라이언트 콘솔에서는 조용히 무시된다.
	 * 2인 PIE 의 클라이언트 창에서도 회복·사망을 테스트할 수 있도록 통로를 연다.
	 */
	UFUNCTION(Server, Reliable)
	void ServerDebugDamage(float Amount);
};
