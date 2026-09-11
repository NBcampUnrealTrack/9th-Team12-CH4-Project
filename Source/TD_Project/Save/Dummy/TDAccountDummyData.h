#pragma once

#include "CoreMinimal.h"
#include "Core/TDAccountSubSystem.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "TDAccountDummyData.generated.h"

USTRUCT(BlueprintType)
struct FTDDummyLocationPreset
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Location") FName Id;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Location", meta=(Categories="Zone")) FGameplayTag ZoneId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Location") FVector Location = FVector::ZeroVector;
};

/** 개발용 초기 계정 데이터. 플레이 중에는 이 에셋을 변경하지 않는다. */
UCLASS(BlueprintType)
class TD_PROJECT_API UTDAccountDummyData : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dummy Accounts", meta=(TitleProperty="LoginId"))
    TArray<FTDDummyAccountRecord> Accounts;

    /** 자주 쓰는 존/좌표를 등록한다. 캐릭터의 Initial Location Preset에서 선택한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Location Presets", meta=(TitleProperty="Id"))
    TArray<FTDDummyLocationPreset> LocationPresets;

    UFUNCTION(BlueprintPure, Category="Dummy Accounts")
    static TArray<FString> GetClassOptions();
    UFUNCTION(BlueprintPure, Category="Dummy Accounts")
    static TArray<FString> GetItemOptions();
    UFUNCTION(BlueprintPure, Category="Dummy Accounts")
    TArray<FString> GetLocationPresetOptions() const;
    UFUNCTION(BlueprintPure, Category="Dummy Accounts")
    TArray<FTDDummyAccountRecord> BuildInitialAccounts() const;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** Project Settings > Game > TD Dummy Accounts */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="TD Dummy Accounts"))
class TD_PROJECT_API UTDAccountDummySettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override { return TEXT("Game"); }
    UPROPERTY(Config, EditAnywhere, Category="Dummy Accounts")
    TSoftObjectPtr<UTDAccountDummyData> InitialAccounts;
    /** 아이템 선택 목록의 원본 DT_ItemDefinition. */
    UPROPERTY(Config, EditAnywhere, Category="Dropdowns")
    TSoftObjectPtr<class UDataTable> ItemTable;
};
