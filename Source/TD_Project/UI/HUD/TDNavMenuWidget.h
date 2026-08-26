#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HUD/Nav/TDNavMenuTypes.h"
#include "TDNavMenuWidget.generated.h"

class UTDHudNavButtonDA;
class UTDNavMenuButtonWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDOnNavMenuRequested, ETDNavMenuType, MenuType);

UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDNavMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// UI Subsystem 없이도 상위 위젯/블루프린트가 메뉴 요청을 받을 수 있습니다.
	UPROPERTY(BlueprintAssignable, Category = "Navigation")
	FTDOnNavMenuRequested OnMenuRequested;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTDNavMenuButtonWidget> InventoryEntry;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTDNavMenuButtonWidget> QuestEntry;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTDNavMenuButtonWidget> CharacterEntry;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTDNavMenuButtonWidget> SkillEntry;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTDNavMenuButtonWidget> PartyEntry;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTDNavMenuButtonWidget> SystemEntry;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Button Style")
		TObjectPtr<UTDHudNavButtonDA> ButtonStyleData;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTDNavMenuButtonWidget> SelectedEntry;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTDNavMenuButtonWidget>> NavEntries;

	void InitializeNavEntries();
	void SelectEntry(UTDNavMenuButtonWidget* NewSelectedEntry);

	UFUNCTION()
	void HandleEntryClicked(UTDNavMenuButtonWidget* ButtonWidget, ETDNavMenuType MenuType);
};
