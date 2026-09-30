#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HUD/Nav/TDNavMenuTypes.h"
#include "TDNavMenuWidget.generated.h"

class UTDButtonStyleDA;
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

	/** 실제 WindowLayer의 열림 상태에 맞춰 Nav 선택 표시를 갱신한다. */
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void SetMenuSelected(ETDNavMenuType MenuType, bool bSelected);

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

	/** 유니온 창 버튼. WBP 에 이 이름으로 배치하지 않으면 버튼 없이 단축키로만 열린다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTDNavMenuButtonWidget> UnionEntry;

	/** 거래소 창 버튼. 위와 같다 — 배치하지 않으면 단축키(M)로만 열린다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTDNavMenuButtonWidget> MarketEntry;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Button Style")
	TObjectPtr<UTDButtonStyleDA> ButtonStyleData;

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
