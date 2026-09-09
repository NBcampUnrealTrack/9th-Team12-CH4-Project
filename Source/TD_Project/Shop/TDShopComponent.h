#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TDShopComponent.generated.h"

/**
 * "이 액터는 상인이다" 를 표시하는 컴포넌트. **NPC 블루프린트에 추가해서 쓴다.**
 *
 * ── 왜 ATDNPCBase 에 필드를 넣지 않았나 ──
 * NPC·대화·퀘스트는 다른 담당의 코드다. 거기에 ShopId 를 넣으면 상점을 고칠 때마다
 * 남의 클래스를 건드리게 된다. 컴포넌트로 두면 BP_NPC_* 를 열어 컴포넌트를 추가하고
 * ShopId 만 적으면 되고, C++ 은 그대로다.
 *
 * 상인이 아닌 액터에 붙여도 된다 — 상점 상자나 자판기 같은 것이 생겨도 그대로 쓴다.
 *
 * ── 여기에 Server RPC 를 두지 않는 이유 ──
 * 이 컴포넌트가 붙은 액터(NPC)는 플레이어가 소유하지 않으므로 클라이언트가 부른
 * Server RPC 가 서버까지 가지 않는다. 거래 요청은 ATDPlayerController 를 통로로 쓴다.
 *
 * 복제하지 않는다. 레벨에 배치된 액터라 클라이언트도 자기 월드에 이미 갖고 있고,
 * ShopId 는 에디터에서 정해진 뒤 바뀌지 않는다.
 */
UCLASS(ClassGroup = (TD), meta = (BlueprintSpawnableComponent))
class TD_PROJECT_API UTDShopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDShopComponent();

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "TD|Shop")
	FName GetShopId() const { return ShopId; }

	/**
	 * 지금 살아 있는 상인 전부. 근접 검증이 이걸 훑는다.
	 *
	 * 월드의 모든 액터를 순회하는 대신 스스로 등록한다. 상인은 두어 명뿐이라
	 * 캐시를 둘 만큼은 아니지만, 액터 전체를 훑으면 거래 한 번에 수천 개를 지나가게 된다.
	 *
	 * **PIE 는 한 프로세스에 서버·클라 월드가 함께 돌므로** 쓰는 쪽에서 반드시
	 * GetWorld() 를 비교해 걸러야 한다. 남의 월드 상인이 섞여 들어온다.
	 */
	static const TArray<TWeakObjectPtr<UTDShopComponent>>& GetAllShops() { return Registry; }

protected:
	/**
	 * DT_Shop 의 RowName. 이 상인이 어느 목록을 파는가.
	 *
	 * 마을마다 목록이 다르므로 EditAnywhere 다 — 상인 BP 를 공유하더라도
	 * 배치한 액터마다 다른 값을 줄 수 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Shop")
	FName ShopId;

private:
	/** 살아 있는 상인 목록. BeginPlay 에서 넣고 EndPlay 에서 뺀다. */
	static TArray<TWeakObjectPtr<UTDShopComponent>> Registry;
};
