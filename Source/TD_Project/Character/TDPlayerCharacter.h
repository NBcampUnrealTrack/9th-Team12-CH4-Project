#pragma once

#include "CoreMinimal.h"
#include "Character/TDCharacterBase.h"
#include "TDPlayerCharacter.generated.h"

/**
 * 플레이어가 조종하는 캐릭터.
 *
 * 스탯 컴포넌트를 소유하지 않는다. 리스폰 시 이 액터는 파괴되므로,
 * 스탯은 PlayerState 쪽에 있고 여기서는 경로만 이어준다.
 */
UCLASS()
class TD_PROJECT_API ATDPlayerCharacter : public ATDCharacterBase
{
	GENERATED_BODY()

public:
	virtual UTDStatComponent* GetStatComponent() const override;

	virtual UTDProgressionComponent* GetProgressionComponent() const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ── GAS 초기화 ────────────────────────────────────────
	// ASC 는 자기가 누구에게 붙었는지(Owner)와 어떤 액터를 대신하는지(Avatar)를 알아야 한다.
	// 서버는 컨트롤러가 빙의할 때, 클라이언트는 PlayerState 복제가 끝날 때 그 정보가 갖춰진다.
	//
	// **둘 중 하나만 하면 반드시 문제가 생긴다.**
	// 서버에서만 하면 클라이언트에서 어트리뷰트가 붙지 않고,
	// 클라에서만 하면 서버 계산이 반영되지 않는다. GAS 에서 가장 흔한 사고다.

	virtual void PossessedBy(AController* NewController) override;

	virtual void OnRep_PlayerState() override;

private:
	/** 서버·클라 양쪽에서 불린다. 여러 번 호출돼도 안전하다. */
	void InitAbilityActorInfo();
};
