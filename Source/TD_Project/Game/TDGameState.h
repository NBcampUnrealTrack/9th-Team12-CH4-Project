#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayTagContainer.h"
#include "TDGameState.generated.h"

/** 존이 바뀌었을 때. 서버·클라이언트 양쪽에서 불린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOnZoneChanged, FGameplayTag, NewZoneId);

class ATDBossCharacter;

/** 전투 중인 보스가 바뀌었을 때. nullptr 면 전투 끝. 서버·클라이언트 양쪽에서 불린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOnActiveBossChanged, ATDBossCharacter*, NewBoss);

/**
 * 모든 클라이언트가 알아야 하는 월드 상태.
 *
 * GameMode 는 서버에만 존재해서 클라이언트에서 GetGameMode() 가 nullptr 이다.
 * 그래서 UI 가 읽어야 하는 정보는 전부 이쪽에 둔다.
 *
 * 나중에 필드 보스 상태나 월드 이벤트가 생기면 컴포넌트로 나눠 붙인다 —
 * GameState 는 복제되므로 붙인 컴포넌트도 클라이언트에서 볼 수 있다.
 */
UCLASS()
class TD_PROJECT_API ATDGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * **월드 전체에 하나뿐인** 존 상태. 낮과 밤, 서버 이벤트처럼 모두에게 같은 것만 담는다.
	 *
	 * 플레이어가 지금 어느 존에 있는지는 여기가 아니라 `ATDPlayerState::CurrentZoneId` 다.
	 * 좌표 텔레포트를 고른 이유가 "각자 다른 존에 있는 구조"이므로(D47),
	 * 그것을 월드에 하나뿐인 값으로 표현하면 A 가 필드로 갈 때 마을에 있는 B 의
	 * 음악까지 바뀐다.
	 *
	 * 지금은 사용처가 없다. 지우지 않는 이유는 월드 단위 상태가 생기면 다시 필요해서다.
	 *
	 * 지역 단위로 묻고 싶으면 상위 태그로 비교하면 된다.
	 *   GetZoneId().MatchesTag(TDTags::Zone_Region1.GetTag())
	 */
	UFUNCTION(BlueprintPure, Category = "TD|World")
	FGameplayTag GetZoneId() const { return ZoneId; }

	/** 서버 전용. 맵을 옮기거나 존을 전환할 때 호출한다. */
	void SetZoneId(FGameplayTag NewZoneId);

	UPROPERTY(BlueprintAssignable, Category = "TD|World")
	FTDOnZoneChanged OnZoneChanged;
	
	/**
	* 지금 전투 중인 보스. 보스 체력바·타이틀 같은 HUD 가 "누구를 보여줄지" 여기서 받는다.
	* 보스가 BeginFight 에 등록하고 사망·리셋에 해제한다. 늦게 접속해도 복제로 받는다.
	*/
	UFUNCTION(BlueprintPure, Category = "TD|Boss")
	ATDBossCharacter* GetActiveBoss() const { return ActiveBoss; }

	/** 서버 전용. 보스 클래스가 부른다. */
	void SetActiveBoss(ATDBossCharacter* NewBoss);

	UPROPERTY(BlueprintAssignable, Category = "TD|Boss")
	FTDOnActiveBossChanged OnActiveBossChanged;

private:
	UFUNCTION()
	void OnRep_ZoneId();

	UPROPERTY(ReplicatedUsing = OnRep_ZoneId)
	FGameplayTag ZoneId;
	
	UFUNCTION()
	void OnRep_ActiveBoss();

	UPROPERTY(ReplicatedUsing = OnRep_ActiveBoss)
	TObjectPtr<ATDBossCharacter> ActiveBoss;
};
