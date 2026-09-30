#pragma once

#include "CoreMinimal.h"
#include "Data/TDQuestTypes.h"
#include "UObject/Interface.h"
#include "TDDialogueSource.generated.h"

class UDataTable;
class UTexture2D;

UINTERFACE(BlueprintType)
class TD_PROJECT_API UTDDialogueSource
	: public UInterface
{
	GENERATED_BODY()
};

class TD_PROJECT_API ITDDialogueSource
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent,
		BlueprintCallable,
		Category = "TD|Dialogue")
	FName GetDialogueSourceId() const;

	UFUNCTION(BlueprintNativeEvent,
		BlueprintCallable,
		Category = "TD|Dialogue")
	ETDQuestTargetType
	GetDialogueQuestTargetType() const;

	UFUNCTION(BlueprintNativeEvent,
		BlueprintCallable,
		Category = "TD|Dialogue")
	FText GetDialogueDisplayName() const;

	UFUNCTION(BlueprintNativeEvent,
		BlueprintCallable,
		Category = "TD|Dialogue")
	TSoftObjectPtr<UTexture2D>
	GetDialoguePortrait() const;

	UFUNCTION(BlueprintNativeEvent,
		BlueprintCallable,
		Category = "TD|Dialogue")
	UDataTable* GetDialogueTable() const;

	UFUNCTION(BlueprintNativeEvent,
		BlueprintCallable,
		Category = "TD|Dialogue")
	bool IsDialogueSourceInRange(
		const AActor* PlayerActor) const;
};