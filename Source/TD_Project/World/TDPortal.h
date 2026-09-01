#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TDPortal.generated.h"

class UBoxComponent;
class APlayerController;

/**
 * 존과 존을 잇는 문. 레벨에 배치해서 쓴다.
 *
 * **판정을 직접 하지 않는다.** 겹침을 감지해 `ATDGameMode::RequestZoneTravel` 에 넘길 뿐이며,
 * 레벨 검증·좌표 결정·ZoneId 갱신은 전부 그쪽이 한다. 여기서 TeleportTo 를 직접 부르면
 * 레벨 제한을 우회하고 BGM·라이팅이 바뀌지 않는다.
 *
 * 목적지의 BGM 이나 라이팅도 알지 않는다 — 그것은 존이 소유한다(D69).
 * 포탈이 아는 것은 "어디로 이어지는가" 와 "어느 자리로 나오는가" 뿐이다.
 *
 * ── 복제하지 않는 이유 ──
 * 레벨에 배치된 액터라 클라이언트도 자기 월드에 이미 갖고 있다. 겹침 판정은
 * 서버에서만 하고(서버가 모든 캐릭터의 위치를 안다), 결과인 좌표 이동은
 * 캐릭터 이동 복제를 타고 자연히 클라이언트에 반영된다.
 */
UCLASS()
class TD_PROJECT_API ATDPortal : public AActor
{
	GENERATED_BODY()

public:
	ATDPortal();

	/** 상호작용형 포탈을 실제로 발동시킨다. 입력 처리가 붙으면 그쪽에서 부른다. */
	UFUNCTION(BlueprintCallable, Category = "TD|Portal")
	void Activate(APlayerController* Player);

	/** 상호작용형인가. UI 가 "F 키로 입장" 안내를 띄울지 판단하는 데 쓴다. */
	UFUNCTION(BlueprintPure, Category = "TD|Portal")
	bool RequiresInteraction() const { return bRequiresInteraction; }

	UFUNCTION(BlueprintPure, Category = "TD|Portal")
	FGameplayTag GetTargetZoneId() const { return TargetZoneId; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	/** 겹침 범위. 아트가 크기를 조정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TD|Portal")
	TObjectPtr<UBoxComponent> TriggerBox;

	/**
	 * 어느 존으로 이어지는가.
	 *
	 * 도착 액터를 지정하더라도 이 값은 필요하다 — 레벨 제한을 어느 존 기준으로 볼지,
	 * CurrentZoneId 에 무엇을 넣을지 정하는 값이기 때문이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Portal")
	FGameplayTag TargetZoneId;

	/**
	 * 도착 지점. 에디터에서 드래그로 지정한다.
	 *
	 * `PlayerStart` 가 아니어도 되며 `TargetPoint` 가 더 가볍다. 지정해 두면
	 * PlayerStartTag 문자열을 포탈과 도착 지점 양쪽에 맞춰 적을 필요가 없어
	 * 오타가 원천적으로 생기지 않는다.
	 *
	 * 비워두면 아래 EntryName 으로, 그것도 없으면 존의 기본 진입점으로 간다.
	 * 포탈마다 목적지가 다르므로 EditInstanceOnly 다 — 블루프린트 기본값에 두면
	 * 실수로 모든 포탈이 같은 곳을 가리키게 된다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "TD|Portal")
	TObjectPtr<AActor> TargetEntryPoint;

	/**
	 * 도착 지점을 이름으로 지정할 때 쓴다. "West" 를 넣으면
	 * PlayerStartTag 가 "<존태그>.West" 인 곳을 찾는다.
	 *
	 * TargetEntryPoint 를 쓰면 필요 없다. 액터를 드래그할 수 없는 상황
	 * (다른 팀원이 나중에 배치할 자리 등)을 위해 남겨둔 경로다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Portal")
	FName EntryName;

	/**
	 * 밟는 즉시가 아니라 키를 눌러야 발동하는가.
	 *
	 * 필드 사이는 자동이 편하지만, 보스방처럼 실수로 들어가면 곤란한 곳은
	 * 확인을 거치는 편이 낫다. 포탈마다 정할 수 있게 둔다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Portal")
	bool bRequiresInteraction = false;

	/**
	 * 같은 플레이어가 다시 발동하기까지의 최소 간격(초).
	 *
	 * 없으면 도착 지점 근처에 반대편 포탈이 있을 때 무한히 왕복한다.
	 * 텔레포트 직후에는 아직 겹침 범위 안에 있을 수도 있어서 필요하다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Portal", meta = (ClampMin = "0.0"))
	float Cooldown = 1.5f;

private:
	/** 이동을 시도하고 결과를 그 플레이어에게만 알린다. */
	void TryTravel(APlayerController* Player);

	/**
	 * 플레이어별 마지막 발동 시각. 포탈 하나가 여러 명을 상대하므로 전역 쿨다운은 쓸 수 없다 —
	 * 한 명이 지나가면 다른 사람이 못 들어가게 된다.
	 *
	 * 접속이 끊긴 컨트롤러가 남을 수 있어 약한 참조로 들고, 만료된 항목은 정리한다.
	 */
	TMap<TWeakObjectPtr<APlayerController>, double> LastUseTimes;
};
