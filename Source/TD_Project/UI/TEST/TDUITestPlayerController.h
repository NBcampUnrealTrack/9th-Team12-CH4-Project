#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TDUITestPlayerController.generated.h"

class UUserWidget;

/** Play 시 인벤토리 UI를 바로 띄우기 위한 임시 PlayerController. */
UCLASS()
class TD_PROJECT_API ATDUITestPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATDUITestPlayerController();

protected:
	virtual void BeginPlay() override;

	/** BeginPlay 때 생성할 루트 UI. BP 자식의 Class Defaults에서 자유롭게 교체할 수 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TD|Test UI")
	TSubclassOf<UUserWidget> StartupWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "TD|Test UI")
	TObjectPtr<UUserWidget> StartupWidget;
};
