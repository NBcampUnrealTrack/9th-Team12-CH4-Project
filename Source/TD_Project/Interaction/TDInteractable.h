#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TDInteractable.generated.h"

class ATDPlayerCharacter;

/**
 * NPC, 보물상자, 아이템 등 F키로 상호작용할 대상이 구현하는 공통 인터페이스.
 *
 * 실제 상호작용 요청은 플레이어가 소유한 UTDInteractionComponent가 서버로 보낸다.
 * NPC나 상자는 클라이언트 소유 Actor가 아니므로 그쪽에 Server RPC를 두면 안 된다.
 */
UINTERFACE(BlueprintType, Blueprintable)
class TD_PROJECT_API UTDInteractable : public UInterface
{
	GENERATED_BODY()
};

class TD_PROJECT_API ITDInteractable
{
	GENERATED_BODY()

public:
	/**
	 * 이 플레이어가 현재 상호작용할 수 있는지 검사한다.
	 *
	 * 서버에서는 거리 외에 퀘스트 조건, 이미 획득한 상자인지 등을 검사한다.
	 * 클라이언트에서는 "F 대화", "F 열기" 안내 표시를 거르는 데 사용할 수 있다.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "TD|Interaction")
	bool CanInteract(ATDPlayerCharacter* Player) const;

	/**
	 * 서버에서만 호출되는 실제 상호작용 함수.
	 *
	 * NPC는 대화 시작, 상자는 아이템 지급 등의 처리를 구현한다.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "TD|Interaction")
	void Interact(ATDPlayerCharacter* Player);

	/** 화면에 표시할 상호작용 문구. 예: "대화하기", "상자 열기" */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "TD|Interaction")
	FText GetInteractionText(ATDPlayerCharacter* Player) const;
};