#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDInventoryWidget.generated.h"

class UTDInventoryComponent;
class UTDInventorySlotData;
class UDataTable;
class UTileView;

/** WBP_Inventory가 상속할 인벤토리 화면의 C++ 부모. */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTDInventoryWidget(const FObjectInitializer& ObjectInitializer);

	/** 현재 컴포넌트 상태를 읽어 0..SlotCapacity-1 슬롯을 다시 만든다. */
	UFUNCTION(BlueprintCallable, Category = "TD|Inventory")
	void RefreshInventory();

	/**
	 * 지정하면 실제 플레이어 인벤토리 대신 해당 아이템 정의 테이블로 테스트 슬롯을 만든다.
	 * nullptr를 전달하면 다시 실제 플레이어 인벤토리를 표시한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TD|Inventory|Test")
	void SetInventoryOverrideTable(UDataTable* InTable);

	UFUNCTION(BlueprintPure, Category = "TD|Inventory")
	UTDInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** WBP 안의 Tile View 이름을 InventoryTileView로 맞춘다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTileView> InventoryTileView;

	/** WBP에서 로딩/빈 상태 등 추가 연출을 갱신할 때 사용한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TD|Inventory", meta = (DisplayName = "On Inventory Refreshed"))
	void BP_OnInventoryRefreshed();

	/** UMG 디자이너에서만 DT_ItemDefinition으로 만든 테스트 인벤토리를 표시한다. 게임 실행 중에는 사용하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Inventory|Test")
	bool bUseVirtualInventory = false;

	/**
	 * 지정된 경우 디자이너와 런타임 모두 실제 인벤토리 대신 이 테이블을 표시한다.
	 * None이면 기존 PlayerState의 InventoryComponent를 사용한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Inventory|Test")
	TObjectPtr<UDataTable> InventoryOverrideTable;

	/** 가상 인벤토리에 사용할 아이템 정의 테이블. 기본값은 DT_ItemDefinition이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Inventory|Test", meta = (EditCondition = "bUseVirtualInventory"))
	TObjectPtr<UDataTable> VirtualItemTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Inventory|Test", meta = (EditCondition = "bUseVirtualInventory", ClampMin = "1", ClampMax = "200"))
	int32 VirtualSlotCapacity = 40;

private:
	void BindInventoryComponent();
	void BuildVirtualInventory();
	void BuildInventoryFromTable(UDataTable* SourceTable);

	UFUNCTION()
	void HandleInventoryChanged();

	UPROPERTY(Transient)
	TObjectPtr<UTDInventoryComponent> InventoryComponent;

	/** Tile View가 사용하는 객체의 수명을 위젯과 함께 유지한다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTDInventorySlotData>> SlotItems;
};
