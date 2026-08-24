#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayTagContainer.h"
#include "TDGameState.generated.h"

/** 존이 바뀌었을 때. 서버·클라이언트 양쪽에서 불린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOnZoneChanged, FGameplayTag, NewZoneId);

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
	 * 현재 존. Zone.Region1.Field01 처럼 계층을 가진다.
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

private:
	UFUNCTION()
	void OnRep_ZoneId();

	UPROPERTY(ReplicatedUsing = OnRep_ZoneId)
	FGameplayTag ZoneId;
};
