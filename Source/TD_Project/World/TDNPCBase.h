#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Interaction/TDDialogueSource.h"
#include "Interaction/TDInteractable.h"
#include "TDNPCBase.generated.h"

class ATDPlayerCharacter;
class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;
class USceneComponent;
class USphereComponent;
class UTDPersonalWorldStateComponent;
class UTDQuestComponent;
class UTDKoreanDailyResetSubsystem;
class UTDShopComponent;
class UWidgetComponent;
class UDataTable;
class UTexture2D;
struct FTDNPCRow;

UCLASS()
class TD_PROJECT_API ATDNPCBase
	: public AActor,
	  public ITDInteractable,
	  public ITDDialogueSource
{
	GENERATED_BODY()

public:
	ATDNPCBase();

	virtual void BeginPlay() override;

	/**
	 * 근처 플레이어를 찾고 NPC 방향을 갱신한다.
	 */
	virtual void Tick(
		float DeltaSeconds) override;

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

	UFUNCTION(BlueprintPure, Category = "TD|NPC")
	FName GetNPCId() const
	{
		return NPCDefinition.RowName;
	}

	UFUNCTION(BlueprintPure, Category = "TD|NPC")
	FText GetNPCDisplayName() const;

	TSoftObjectPtr<UTexture2D>
	GetNPCPortrait() const;

	bool IsPlayerWithinDialogueDistance(
		const AActor* PlayerActor) const;

	bool CanReceiveGifts() const;

	/** 정상적인 대화 종료 후 강화 기능을 제공하는 NPC인지 확인합니다. */
	UFUNCTION(BlueprintPure, Category = "TD|NPC|Enhance")
	bool IsEnhanceNPC() const;

	/** DT_NPC에서 상점 사용이 켜져 있고 ShopId도 지정된 NPC인지 확인합니다. */
	UFUNCTION(BlueprintPure, Category = "TD|NPC|Shop")
	bool IsShopNPC() const;

	UFUNCTION(BlueprintPure, Category = "TD|NPC|Shop")
	FName GetShopId() const;

	UTDShopComponent* GetShopComponent() const
	{
		return ShopComponent;
	}
	
	int32 GetGiftAffectionValue(
		FName ItemId) const;

	FText GetGiftThankYouText() const;

	/**
	 * 대화 또는 챕터 연출 중 퀘스트 마커를
	 * 임시로 숨긴다.
	 */
	void SetQuestMarkerSuppressed(
		bool bSuppressed);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	TObjectPtr<UPaperFlipbookComponent>
		SpriteComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	TObjectPtr<UPaperZDAnimationComponent>
		AnimationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	TObjectPtr<USphereComponent>
		InteractionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	TObjectPtr<UWidgetComponent>
		QuestMarkerComponent;

	/** 모든 NPC가 가지되, DT_NPC에서 상점 사용이 켜진 경우에만 상인으로 등록됩니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
		Category = "TD|NPC|Shop")
	TObjectPtr<UTDShopComponent> ShopComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	FDataTableRowHandle NPCDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC")
	TObjectPtr<UDataTable> DialogueTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC",
		meta = (ClampMin = "150.0"))
	float DialogueContinueDistance = 350.0f;

	/**
	 * 이 거리 안에 살아 있는 플레이어가 있으면
	 * NPC가 플레이어를 바라본다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC|Facing",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "1000.0"))
	float PlayerLookAtDistance = 350.0f;

	/**
	 * NPC가 목표 방향으로 회전하는 속도.
	 *
	 * 값이 작으면 천천히,
	 * 값이 크면 빠르게 방향이 바뀐다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC|Facing",
		meta = (
			ClampMin = "0.1",
			UIMin = "0.1",
			UIMax = "20.0"))
	float FacingRotationInterpSpeed = 6.0f;

	/**
	 * 월드 방향과 PaperZD 방향 사이의 각도 차이를 보정한다.
	 *
	 * 현재 프로젝트에서는 90도 어긋나므로
	 * 기본값을 90으로 설정한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC|Facing",
		meta = (
			ClampMin = "-180.0",
			ClampMax = "180.0",
			UIMin = "-180.0",
			UIMax = "180.0"))
	float FacingYawOffsetDegrees = 90.0f;

	/**
	 * 플레이어가 멀어졌을 때 NPC가
	 * 처음 배치된 방향으로 돌아갈지 결정한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "TD|NPC|Facing")
	bool bReturnToInitialFacing = true;

private:
	const FTDNPCRow* GetDefinitionRow() const;

	FName ResolveDialogueStartRow(
		const ATDPlayerCharacter* Player) const;

	FName FindInProgressDialogueRow(
		const UTDQuestComponent* Quest) const;

	void TryBindToLocalPlayerState();

	UFUNCTION()
	void HandleLocalStateChanged();

	void HandleKoreanDayChanged();

	void RefreshLocalPresentation();

	void SetLocalPresentationHidden(
		bool bShouldHide);

	/**
	 * 게임 시작 당시 맵에 배치되어 있던
	 * NPC의 기본 회전값이다.
	 */
	FRotator InitialFacingRotation =
		FRotator::ZeroRotator;

	UPROPERTY(Transient)
	TObjectPtr<UTDPersonalWorldStateComponent>
		LocalPersonalState;

	UPROPERTY(Transient)
	TObjectPtr<UTDQuestComponent>
		LocalQuestComponent;

	UPROPERTY(Transient)
	TObjectPtr<UTDKoreanDailyResetSubsystem>
		DailyResetSubsystem;

	/**
	 * true면 퀘스트 상태와 상관없이
	 * NPC 머리 위 마커를 임시로 숨긴다.
	 */
	bool bQuestMarkerSuppressed = false;

	FTimerHandle BindRetryTimerHandle;
};
