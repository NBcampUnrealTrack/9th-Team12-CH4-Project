#include "Shop/TDShopComponent.h"

TArray<TWeakObjectPtr<UTDShopComponent>> UTDShopComponent::Registry;

UTDShopComponent::UTDShopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTDShopComponent::BeginPlay()
{
	Super::BeginPlay();

	// 비워두면 이 상인은 아무것도 팔지 않는다. 조용히 넘어가면 "상점이 안 열린다" 로만
	// 보이고 원인을 찾기 어렵다.
	if (ShopId.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s: TDShopComponent 의 ShopId 가 비어 있다. DT_Shop 의 행 이름을 넣을 것."),
			*GetNameSafe(GetOwner()));
	}

	Registry.AddUnique(this);
}

void UTDShopComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Registry.Remove(this);

	// 파괴된 항목이 쌓이지 않게 함께 정리한다. 목록이 짧아 비용이 없다.
	Registry.RemoveAll([](const TWeakObjectPtr<UTDShopComponent>& Entry)
	{
		return !Entry.IsValid();
	});

	Super::EndPlay(EndPlayReason);
}
