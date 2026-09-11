#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FieldNotificationId.h"
#include "Items/TDItemTypes.h"
#include "UI/Common/ItemSlot/TDItemSlotVisualWidget.h"
#include "TDCharacterContentWidget.generated.h"

class UTDPlayerStatsViewModel;
class UButton;
class UTDTextBlock;
class UImage;
class UTexture2D;
class UWidgetSwitcher;
class UTDItemUseComponent;
class UTDInventoryComponent;

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDCharacterContentWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TD|Character")
	void SetViewModel(UTDPlayerStatsViewModel* InViewModel);

	UFUNCTION(BlueprintPure, Category = "TD|Character")
	UTDPlayerStatsViewModel* GetViewModel() const { return ViewModel; }

	UFUNCTION(BlueprintCallable, Category = "TD|Character")
	void SetCharacterPortrait(UTexture2D* InTexture);

	/** 장신구 표시 전용. 서버 장착 칸 0..5를 UI Slot1..Slot6에 대응한다. */
	UFUNCTION(BlueprintCallable, Category = "TD|Character", meta = (ClampMin = "0", ClampMax = "5"))
	void SetEquipmentVisual(int32 EquipmentSlotIndex, const FTDItemSlotVisualData& Data);

	/** 소유 플레이어에게 복제된 장착 상태를 읽는다. 서버 요청/아이템 이동은 하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "TD|Character")
	void RefreshEquipment();

	/** UI에서 표시 중인 장비만 해제 요청한다. true는 요청 전송이며 서버 성공 응답이 아니다. */
	bool RequestUnequipEquipment(int32 EquipmentSlotIndex);

	UFUNCTION(BlueprintCallable, Category = "TD|Character", meta = (ClampMin = "0", ClampMax = "1"))
	void SetActiveInfoTab(int32 TabIndex);

	UFUNCTION(BlueprintPure, Category = "TD|Character")
	int32 GetActiveInfoTab() const;

protected:
 UFUNCTION(BlueprintImplementableEvent, Category = "TD|Character")
 void OnStatsPresentation(UTDPlayerStatsViewModel* Data, bool bReady);

 UFUNCTION(BlueprintImplementableEvent, Category = "TD|Character")
 void OnInfoTabChanged(int32 TabIndex);

 UFUNCTION(BlueprintImplementableEvent, Category = "TD|Character")
 void OnPresentationInitialized();

 /** WBP가 지정한 대상과 문구를 기존 공용 툴팁에 연결한다. */
 UFUNCTION(BlueprintCallable, Category = "TD|Character", meta = (AutoCreateRefTerm = "Title,Description"))
 void ConfigureTextTooltip(FName HostName, const FText& Title, const FText& Description);

 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TD|Character|Text")
 FText EquippedActionHint;

	/** 이미지 표시와 숨김은 WBP에서 처리한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TD|Character")
	void OnPortraitChanged(UTexture2D* Texture, bool bHasPortrait);

	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Character")
	TObjectPtr<UTexture2D> PortraitTexture;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> CharacterPortrait;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> CharacterNameText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> LevelValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> HealthValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> ManaValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> AttackValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DefenseValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> CriticalValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> BasicTabButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> DetailTabButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> InfoSwitcher;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailCombatPowerValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailPhysicalAttackValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailMagicalAttackValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailCriticalChanceValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailCriticalDamageValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailArmorPenetrationValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailBossDamageValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailDefenseValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailDamageReductionValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailMaxHealthValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailHealthRegenValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailMaxManaValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailManaRegenValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailMoveSpeedValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDTextBlock> DetailCooldownRecoveryValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDItemSlotVisualWidget> Slot1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDItemSlotVisualWidget> Slot2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDItemSlotVisualWidget> Slot3;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDItemSlotVisualWidget> Slot4;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDItemSlotVisualWidget> Slot5;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDItemSlotVisualWidget> Slot6;

private:
 int32 ActiveInfoTab = 0;
	UPROPERTY(Transient)
	TObjectPtr<UTDItemUseComponent> EquipmentSource;
	UPROPERTY(Transient)
	TObjectPtr<UTDInventoryComponent> EquipmentInventory;
	FTimerHandle EquipmentSourceTimer;
	FTimerHandle EquipmentRefreshTimer;
	TArray<FTDItemInstance> DisplayedEquipment;
	TWeakObjectPtr<UTDItemUseComponent> PendingUnequipSource;
	FTDItemInstance PendingUnequipItem;
	bool bUnequipPending = false;
	double UnequipRequestedAt = -1.0;
	void UpdateUnequipPending();
	void CheckEquipmentSource();
	void UnbindEquipmentSource();
	UFUNCTION()
	void QueueEquipmentRefresh();

	UPROPERTY(Transient)
	TObjectPtr<UTDPlayerStatsViewModel> ViewModel;

	void RefreshStats();
	void OnFieldChanged(UObject* Object, UE::FieldNotification::FFieldId Field);

	UFUNCTION()
	void HandleBasicTabClicked();

	UFUNCTION()
	void HandleDetailTabClicked();
};
