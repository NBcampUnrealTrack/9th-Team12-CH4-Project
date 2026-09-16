#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "TDGameInstance.generated.h"

/**
 * 프로그램이 켜져 있는 동안 계속 살아 있는 객체. 레벨을 바꿔도 파괴되지 않는다.
 *
 * 서버와 각 클라이언트가 **각자 자기 것을 하나씩** 가지며 복제되지 않는다.
 * 따라서 캐릭터의 레벨이나 아이템처럼 권위가 필요한 값을 여기 두면 안 된다 —
 * 클라이언트가 자기 것을 마음대로 고칠 수 있기 때문이다.
 *
 * 지금은 직접 IP 접속만 담당한다. 데이터 테이블을 모아 관리할
 * UTDDataSubsystem 도 나중에 여기 붙는다.
 */
UCLASS(Config=Game, DefaultConfig)
class TD_PROJECT_API UTDGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/**
	 * GAS 전역 데이터를 초기화한다.
	 *
	 * 이걸 부르지 않으면 TargetData(어빌리티가 대상 정보를 주고받는 구조)를 쓰는 순간
	 * 크래시한다. 증상이 나타나는 시점이 한참 뒤라 원인을 찾기 어려운 종류의 사고다.
	 */
	virtual void Init() override;

	virtual void Shutdown() override;

	/**
	 * 지정한 주소의 서버로 접속한다.
	 *
	 * 하마치를 쓰면 가상 IP 를 그대로 넣으면 된다.
	 *   127.0.0.1:7777      같은 PC 에서 띄운 서버
	 *   25.10.20.30:7777    하마치 가상 IP
	 *
	 * @return 접속을 시도했으면 true. 실제 성공 여부는 이후 네트워크 결과로 갈린다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TD|Network")
	bool ConnectToServer(const FString& Address);

private:
	/** 패키징된 클라이언트가 시작할 때 기본 서버로 자동 접속할지 여부. */
	UPROPERTY(Config)
	bool bAutoConnectOnStartup = false;

	/** 바로가기 실행 인자가 없을 때 접속할 기본 서버 주소. */
	UPROPERTY(Config)
	FString DefaultServerAddress;

	/** 맵 이동 후 같은 서버로 반복 접속하는 것을 막는다. */
	bool bAutoConnectAttempted = false;

	/**
	 * 맵이 로드될 때마다 저장된 옵션을 실제로 반영한다.
	 *
	 * Init 에서 한 번만 하면 안 된다. 그 시점에는 월드가 아직 없어 오디오 적용이
	 * 조용히 빠져나가고, 볼륨이 기본값으로 들린 채 게임이 시작된다.
	 *
	 * 맵마다 다시 부르는 것은 낭비가 아니다 — 오디오 디바이스가 월드에 붙어 있어
	 * 맵이 바뀌면 새 월드에 값을 다시 밀어넣어야 하기 때문이다. 값 덮어쓰기라
	 * 여러 번 불러도 누적되지 않는다.
	 *
	 * 존 이동에는 불리지 않는다. 좌표 텔레포트라 맵 로드가 없기 때문이며(D47),
	 * 그쪽은 볼륨이 유지되는 것이 옳다.
	 */
	void HandlePostLoadMap(UWorld* LoadedWorld);

	FDelegateHandle PostLoadMapHandle;
};
