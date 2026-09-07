#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TDWidgetAuthoringLibrary.generated.h"

class UWidgetBlueprint;
class UWidget;
class UUserWidget;

/** Python에 노출되지 않은 WidgetTree 편집과 PNG 검증용. 패키징된 게임에는 포함되지 않는다. */
UCLASS()
class TD_PROJECTEDITOR_API UTDWidgetAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "TD|Editor|Widget")
	static bool SetWidgetRoot(UWidgetBlueprint* Blueprint, UWidget* Root);
	UFUNCTION(BlueprintCallable, Category = "TD|Editor|Widget")
	static void SetNamedSlotContent(UUserWidget* Widget, FName SlotName, UWidget* Content);
	UFUNCTION(BlueprintCallable, Category = "TD|Editor|Widget")
	static UWidget* FindWidget(UUserWidget* Widget, FName Name);
	UFUNCTION(BlueprintCallable, Category = "TD|Editor|Widget")
	static bool RenderWidgetPNG(UUserWidget* Widget, FVector2D Size, const FString& Filename);
};
