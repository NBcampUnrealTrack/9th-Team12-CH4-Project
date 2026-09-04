#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FieldNotificationId.h"
#include "UI/Common/ItemSlot/TDItemSlotVisualWidget.h"
#include "TDCharacterContentWidget.generated.h"

class UTDPlayerStatsViewModel;
class UTextBlock;
class UImage;
class UTexture2D;

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

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Character")
	TObjectPtr<UTexture2D> PortraitTexture;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> CharacterPortrait;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CharacterNameText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> LevelValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ManaValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> AttackValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> DefenseValueText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CriticalValueText;

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
	UPROPERTY(Transient)
	TObjectPtr<UTDPlayerStatsViewModel> ViewModel;

	void RefreshStats();
	void OnFieldChanged(UObject* Object, UE::FieldNotification::FFieldId Field);
};
