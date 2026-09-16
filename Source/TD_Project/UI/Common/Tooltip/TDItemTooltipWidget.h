#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/TDItemTypes.h"
#include "UI/Common/Typography/TDTypographyThemeDA.h"
#include "TDItemTooltipWidget.generated.h"

class UDataTable;
class UImage;
class UTextBlock;
class UVerticalBox;
class UTexture2D;
class UTDItemUseComponent;

/** 아이템/스킬의 공용 표시 행. 아직 없는 아이콘은 비워 두면 된다. */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDTooltipLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
	FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
	FText Value;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
	TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
	FLinearColor ValueColor = FLinearColor(0.45f, 0.95f, 0.50f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
	bool bSectionHeader = false;
};

/** 강화나 서버 로직을 포함하지 않는 표시 전용 데이터. */
USTRUCT(BlueprintType)
struct TD_PROJECT_API FTDTooltipData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
	FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
	FText Subtitle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
	TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
	FLinearColor TitleColor = FLinearColor(0.2f, 0.7f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
	FText Requirement;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
	bool bRequirementUnmet = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip")
	TArray<FTDTooltipLine> Lines;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip", meta = (MultiLine = "true"))
	FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tooltip", meta = (MultiLine = "true"))
	FText Footer;
};

/** WBP_ItemTooltip의 부모. 동일한 카드에 향후 스킬 표시 데이터도 전달할 수 있다. */
UCLASS(Blueprintable)
class TD_PROJECT_API UTDItemTooltipWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** 정의 테이블 Override는 인벤토리 미리보기에서만 사용한다. */
	UFUNCTION(BlueprintCallable, Category = "TD|Tooltip")
	bool SetItem(FName ItemId, int32 Count, FText Hint, UDataTable* DefinitionTable = nullptr, int32 EnhanceLevel = 0);

	/**
	 * 개체 하나를 그대로 표시한다. 강화 단계와 **추가 옵션**이 함께 들어온다.
	 *
	 * SetItem 에 옵션 인자를 덧붙이지 않은 이유는 두 가지다. UFUNCTION 은 TArray 기본값을
	 * 지원하지 않아 기존 호출이 전부 깨지고, 개체 정보가 늘어날 때마다 인자가 또 붙는다.
	 * 아이템 개체를 아는 쪽(인벤토리·장착 칸)은 이쪽을 쓴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "TD|Tooltip")
	bool SetItemInstance(const FTDItemInstance& Item, FText Hint, UDataTable* DefinitionTable = nullptr);

	UFUNCTION(BlueprintCallable, Category = "TD|Tooltip")
	void SetTooltipData(const FTDTooltipData& InData);

	UFUNCTION(BlueprintPure, Category = "TD|Tooltip")
	FTDTooltipData GetTooltipData() const { return Data; }

	UFUNCTION(BlueprintCallable, Category = "TD|Tooltip")
	bool RefreshItem();

	/** Host에 이미 생성된 공용 툴팁이 있으면 재사용. ItemId=None은 해제. */
	static void AttachItem(UUserWidget* Host, FName ItemId, int32 Count,
		const FText& Hint = FText::GetEmpty(), UDataTable* DefinitionTable = nullptr, int32 EnhanceLevel = 0);

	/** AttachItem 과 같지만 추가 옵션까지 함께 붙인다. 개체를 아는 칸이 이쪽을 쓴다. */
	static void AttachItemInstance(UUserWidget* Host, const FTDItemInstance& Item,
		const FText& Hint = FText::GetEmpty(), UDataTable* DefinitionTable = nullptr);

	static void ClearItemTooltip(UUserWidget* Host);

	/** 스탯 등 텍스트 설명도 설정된 공용 카드로 표시한다. 빈 설명은 해제. */
	static void AttachData(UUserWidget* Owner, UWidget* Host, const FTDTooltipData& InData);

	static void AttachText(UUserWidget* Owner, UWidget* Host, const FText& Title, const FText& Description);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> TooltipIcon;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TooltipTitle;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TooltipSubtitle;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TooltipRequirement;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UVerticalBox> TooltipLines;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TooltipDescription;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TooltipFooter;

	/** 동적 행도 TD Text의 테마/역할을 따른다. 폰트는 DA_Font의 CTS에서 편집한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TD|Tooltip")
	ETDTextStyleRole RowTextStyleRole = ETDTextStyleRole::Body;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TD|Tooltip")
	FName RowCustomStyleName = NAME_None;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TD|Tooltip")
	FLinearColor BodyColor = FLinearColor(0.85f, 0.9f, 0.96f);

private:
	/** AttachItem 과 AttachItemInstance 의 공통 본문. 툴팁 생성·재사용·해제를 한곳에서 처리한다. */
	static void AttachItemInternal(UUserWidget* Host, const FTDItemInstance& Item,
		const FText& Hint, UDataTable* DefinitionTable);

	void RefreshContent();
	void BindEquipmentSource(UTDItemUseComponent* Source);
	void RefreshEquipmentTooltip();
	UFUNCTION()
	void HandleEquipmentChanged();
	TWeakObjectPtr<UTDItemUseComponent> EquipmentSource;
	FTimerHandle EquipmentRefreshTimer;

	UPROPERTY(Transient)
	FTDTooltipData Data;
	UPROPERTY(Transient)
	FName SourceItemId;
	int32 SourceEnhanceLevel = 0;
	/** 개체에 굴려진 추가 옵션. SetItem 으로 들어온 경로에서는 비어 있다. */
	UPROPERTY(Transient)
	TArray<FTDItemOption> SourceOptions;
	UPROPERTY(Transient)
	int32 SourceCount = 0;
	UPROPERTY(Transient)
	FText SourceHint;
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> SourceDefinitionTable;
};
