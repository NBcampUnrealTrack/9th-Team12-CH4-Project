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
UCLASS()
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
};
