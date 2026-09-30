#include "Shop/TDShopComponent.h"

TArray<TWeakObjectPtr<UTDShopComponent>> UTDShopComponent::Registry;

UTDShopComponent::UTDShopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTDShopComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!ShopId.IsNone())
	{
		Registry.AddUnique(this);
	}
}

void UTDShopComponent::SetShopId(FName InShopId)
{
	Registry.Remove(this);
	ShopId = InShopId;

	if (HasBegunPlay() && !ShopId.IsNone())
	{
		Registry.AddUnique(this);
	}
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
