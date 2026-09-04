#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Interaction/TDInteractable.h"
#include "TDTreasureChest.generated.h"

class ATDPlayerCharacter;
class UPaperFlipbookComponent;
class USceneComponent;
class USphereComponent;
class UTDPersonalWorldStateComponent;
struct FTDTreasureChestRow;

/**
 * 플레이어별로 독립 획득하는 고정 보물상자.
 *
 * Actor 자체는 레벨에 하나만 배치한다.
 * 획득 상태는 각 PlayerState가 따로 소유한다.
 *
 * 절대 서버에서 DestroyActor 또는 SetLifeSpan을 호출하면 안 된다.
 */
UCLASS()
class TD_PROJECT_API ATDTreasureChest
	: public AActor,
	  public ITDInteractable
{
	GENERATED_BODY()

public:
	ATDTreasureChest();

	virtual void BeginPlay() override;

	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

	// ── ITDInteractable ───────────────────────────────────

	virtual bool CanInteract_Implementation(
		ATDPlayerCharacter* Player) const override;

	virtual void Interact_Implementation(
		ATDPlayerCharacter* Player) override;

	virtual FText GetInteractionText_Implementation(
		ATDPlayerCharacter* Player) const override;

	// ── 로컬 표현 ─────────────────────────────────────────

	/**
	 * 이 클라이언트에서만 개봉 연출을 시작한다.
	 *
	 * 서버가 성공한 플레이어의 PlayerController에만 Client RPC를 보내 호출한다.
	 */
	void PlayClaimedPresentation(float DisappearDelay);

	UFUNCTION(BlueprintPure, Category = "TD|Treasure")
	FName GetChestId() const
	{
		return ChestDefinition.RowName;
	}

	UFUNCTION(BlueprintPure, Category = "TD|Treasure")
	UPaperFlipbookComponent* GetSpriteComponent() const
	{
		return SpriteComponent;
	}

protected:
	/**
	 * 개봉에 성공했을 때 호출되는 BP 이벤트.
	 *
	 * BP에서 열린 상자 플립북, 사운드, 이펙트만 재생한다.
	 * 5초 타이머와 숨김 처리는 C++가 담당한다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "TD|Treasure",
		DisplayName = "On Chest Claimed")
	void BP_OnChestClaimed(float DisappearDelay);

private:
	const FTDTreasureChestRow* GetDefinitionRow() const;

	/** 로컬 PlayerState가 준비될 때까지 짧게 재시도한다. */
	void TryBindToLocalPersonalState();

	UFUNCTION()
	void HandlePersonalWorldStateChanged();

	void RefreshLocalPresentation();
	
	void SetLocalPresentationHidden(bool bInHidden);

	void FinishClaimedPresentation();

	// ── 컴포넌트 ──────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperFlipbookComponent> SpriteComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|Treasure",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> InteractionSphere;

	/**
	 * DT_TreasureChest의 행.
	 *
	 * RowName이 ChestId이고 DataTable에는 보상·조건·사라짐 시간이 들어 있다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
		Category = "TD|Treasure",
		meta = (AllowPrivateAccess = "true"))
	FDataTableRowHandle ChestDefinition;

	/** 이 클라이언트가 소유한 플레이어의 개인 상태 */
	UPROPERTY(Transient)
	TObjectPtr<UTDPersonalWorldStateComponent> LocalPersonalState;

	FTimerHandle BindRetryTimerHandle;
	FTimerHandle LocalHideTimerHandle;

	bool bPlayingClaimPresentation = false;
};