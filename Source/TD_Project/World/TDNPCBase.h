#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Interaction/TDInteractable.h"
#include "TDNPCBase.generated.h"

class ATDPlayerCharacter;
class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;
class USceneComponent;
class USphereComponent;
class UTDPersonalWorldStateComponent;
class UDataTable;
class UTexture2D;
struct FTDNPCRow;

UCLASS()
class TD_PROJECT_API ATDNPCBase
	: public AActor,
	  public ITDInteractable
{
	GENERATED_BODY()

public:
	ATDNPCBase();

	virtual void BeginPlay() override;

	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

	virtual bool CanInteract_Implementation(
		ATDPlayerCharacter* Player) const override;

	virtual void Interact_Implementation(
		ATDPlayerCharacter* Player) override;

	virtual FText GetInteractionText_Implementation(
		ATDPlayerCharacter* Player) const override;

	UFUNCTION(BlueprintPure, Category = "TD|NPC")
	FName GetNPCId() const
	{
		return NPCDefinition.RowName;
	}

	UFUNCTION(BlueprintPure, Category = "TD|NPC")
	FText GetNPCDisplayName() const;

	TSoftObjectPtr<UTexture2D> GetNPCPortrait() const;

	bool IsPlayerWithinDialogueDistance(
		const AActor* PlayerActor) const;

	UFUNCTION(BlueprintPure, Category = "TD|NPC")
	UPaperFlipbookComponent* GetSpriteComponent() const
	{
		return SpriteComponent;
	}

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	TObjectPtr<UPaperFlipbookComponent> SpriteComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	TObjectPtr<UPaperZDAnimationComponent> AnimationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	TObjectPtr<USphereComponent> InteractionSphere;

	/** DT_NPC의 행 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	FDataTableRowHandle NPCDefinition;

	/** 모든 NPC가 함께 사용하는 DT_Dialogue */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	TObjectPtr<UDataTable> DialogueTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC",
		meta = (ClampMin = "150.0"))
	float DialogueContinueDistance = 350.0f;

private:
	const FTDNPCRow* GetDefinitionRow() const;

	FName ResolveDialogueStartRow(
		const ATDPlayerCharacter* Player) const;

	void TryBindToLocalPersonalState();

	UFUNCTION()
	void HandlePersonalWorldStateChanged();

	void RefreshLocalPresentation();
	void SetLocalPresentationHidden(bool bInHidden);
	
	UPROPERTY(Transient)
	TObjectPtr<UTDPersonalWorldStateComponent> LocalPersonalState;

	FTimerHandle BindRetryTimerHandle;
};