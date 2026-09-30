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
class UTDKoreanDailyResetSubsystem;
struct FTDTreasureChestRow;
struct FTDChestItemReward;

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
		const EEndPlayReason::Type
			EndPlayReason) override;

	virtual bool CanInteract_Implementation(
		ATDPlayerCharacter* Player) const override;

	virtual void Interact_Implementation(
		ATDPlayerCharacter* Player) override;

	virtual FText
	GetInteractionText_Implementation(
		ATDPlayerCharacter* Player) const override;

	void PlayClaimedPresentation(
		float DisappearDelay);

	UFUNCTION(BlueprintPure,
		Category = "TD|Treasure")
	FName GetChestId() const
	{
		return ChestDefinition.RowName;
	}

protected:
	UFUNCTION(BlueprintImplementableEvent,
		Category = "TD|Treasure",
		DisplayName = "On Chest Claimed")
	void BP_OnChestClaimed(
		float DisappearDelay);

private:
	const FTDTreasureChestRow*
	GetDefinitionRow() const;

	void RollRandomRewards(
		const FTDTreasureChestRow& Definition,
		TArray<FTDChestItemReward>&
			OutRewards) const;

	bool CanFitAllItemRewards(
		const TArray<FTDChestItemReward>&
			Rewards,
		class UTDInventoryComponent*
			Inventory) const;

	void TryBindToLocalState();

	UFUNCTION()
	void HandlePersonalStateChanged();

	void HandleKoreanDayChanged();

	void RefreshLocalPresentation();
	void SetLocalPresentationHidden(bool bShouldHide);
	void FinishClaimedPresentation();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TD|Treasure", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TD|Treasure", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperFlipbookComponent>
		SpriteComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TD|Treasure", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent>
		InteractionSphere;

	UPROPERTY(EditInstanceOnly)
	FDataTableRowHandle ChestDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UTDPersonalWorldStateComponent>
		LocalPersonalState;

	UPROPERTY(Transient)
	TObjectPtr<UTDKoreanDailyResetSubsystem>
		DailyResetSubsystem;

	FTimerHandle BindRetryTimerHandle;
	FTimerHandle LocalHideTimerHandle;

	bool bPlayingClaimPresentation = false;
};