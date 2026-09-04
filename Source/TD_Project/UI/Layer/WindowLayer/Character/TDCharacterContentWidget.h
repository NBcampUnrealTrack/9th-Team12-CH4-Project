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

UENUM(BlueprintType)
enum class ETDCharacterEquipmentSlot : uint8
{
	Weapon, Necklace, Ring, Crown, Dress, Shoes
};

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

	/** 장착 시스템이 준비되면 실제 장착 아이템의 표시 데이터만 전달한다. */
	UFUNCTION(BlueprintCallable, Category = "TD|Character")
	void SetEquipmentVisual(ETDCharacterEquipmentSlot EquipmentSlot, const FTDItemSlotVisualData& Data);

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
	TObjectPtr<UTDItemSlotVisualWidget> WeaponSlot;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDItemSlotVisualWidget> NecklaceSlot;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDItemSlotVisualWidget> RingSlot;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDItemSlotVisualWidget> CrownSlot;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDItemSlotVisualWidget> DressSlot;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTDItemSlotVisualWidget> ShoesSlot;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTDPlayerStatsViewModel> ViewModel;

	void RefreshStats();
	void OnFieldChanged(UObject* Object, UE::FieldNotification::FFieldId Field);
};
