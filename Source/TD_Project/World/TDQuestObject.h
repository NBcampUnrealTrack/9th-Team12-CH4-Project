#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Interaction/TDDialogueSource.h"
#include "Interaction/TDInteractable.h"
#include "TDQuestObject.generated.h"

class ATDPlayerCharacter;
class UPaperFlipbookComponent;
class USceneComponent;
class USphereComponent;
class UWidgetComponent;
class UTDQuestComponent;
class UTDPersonalWorldStateComponent;
class UTDKoreanDailyResetSubsystem;
class UDataTable;
class UTexture2D;
struct FTDQuestObjectRow;

UCLASS()
class TD_PROJECT_API ATDQuestObject
	: public AActor,
	  public ITDInteractable,
	  public ITDDialogueSource
{
	GENERATED_BODY()

public:
	ATDQuestObject();

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

	virtual FName
	GetDialogueSourceId_Implementation()
		const override;

	virtual ETDQuestTargetType
	GetDialogueQuestTargetType_Implementation()
		const override;

	virtual FText
	GetDialogueDisplayName_Implementation()
		const override;

	virtual TSoftObjectPtr<UTexture2D>
	GetDialoguePortrait_Implementation()
		const override;

	virtual UDataTable*
	GetDialogueTable_Implementation()
		const override;

	virtual bool
	IsDialogueSourceInRange_Implementation(
		const AActor* PlayerActor) const override;

	UFUNCTION(BlueprintPure,
		Category = "TD|QuestObject")
	FName GetQuestObjectId() const
	{
		return ObjectDefinition.RowName;
	}

private:
	const FTDQuestObjectRow*
	GetDefinitionRow() const;

	FName ResolveDialogueStartRow(
		const ATDPlayerCharacter* Player) const;

	FName FindInProgressDialogueRow(
		const UTDQuestComponent* Quest) const;

	bool HasRelevantQuest(
		const UTDQuestComponent* Quest) const;

	void TryBindToLocalPlayerState();

	UFUNCTION()
	void HandleLocalStateChanged();

	void HandleKoreanDayChanged();

	void RefreshLocalPresentation();
	void SetLocalPresentationHidden(bool bShouldHide);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPaperFlipbookComponent>
		SpriteComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent>
		InteractionSphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UWidgetComponent>
		QuestMarkerComponent;

	UPROPERTY(EditInstanceOnly)
	FDataTableRowHandle ObjectDefinition;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UDataTable> DialogueTable;

	UPROPERTY(Transient)
	TObjectPtr<UTDQuestComponent>
		LocalQuestComponent;

	UPROPERTY(Transient)
	TObjectPtr<UTDPersonalWorldStateComponent>
		LocalPersonalState;

	UPROPERTY(Transient)
	TObjectPtr<UTDKoreanDailyResetSubsystem>
		DailyResetSubsystem;

	FTimerHandle BindRetryTimerHandle;
};