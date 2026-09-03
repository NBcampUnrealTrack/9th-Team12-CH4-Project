#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TDGameplayTags.h"
#include "Data/TDItemRow.h"
#include "Data/TDItemUseEffectRow.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Items/TDInventoryComponent.h"
#include "Items/TDItemUseComponent.h"
#include "Items/TDQuickSlotComponent.h"
#include "Misc/ScopeExit.h"
#include "Player/TDPlayerState.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTDQuickSlotTest, "TD.QuickSlot.RegisterAndUse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDQuickSlotTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	ATDPlayerState* State = World->SpawnActor<ATDPlayerState>();
	if (!TestNotNull(TEXT("Player state"), State)) return false;
	UTDInventoryComponent* Inventory = State->GetInventoryComponent();
	UTDQuickSlotComponent* Quick = State->GetQuickSlotComponent();
	UTDItemUseComponent* ItemUse = State->GetItemUseComponent();

	UDataTable* Definitions = NewObject<UDataTable>(Inventory);
	Definitions->RowStruct = FTDItemRow::StaticStruct();
	FTDItemRow Consumable;
	Consumable.ItemType = TDTags::Item_Type_Consumable.GetTag();
	Consumable.bStackable = true;
	Consumable.MaxStackSize = 99;
	Definitions->AddRow(TEXT("Expand"), Consumable);
	Definitions->AddRow(TEXT("NotOwned"), Consumable);
	FTDItemRow Equipment;
	Equipment.ItemType = TDTags::Item_Type_Accessory.GetTag();
	Definitions->AddRow(TEXT("Equipment"), Equipment);
	FObjectPropertyBase* ItemTable = FindFProperty<FObjectPropertyBase>(Inventory->GetClass(), TEXT("ItemTable"));
	if (!TestNotNull(TEXT("Item table property"), ItemTable)) return false;
	ItemTable->SetObjectPropertyValue_InContainer(Inventory, Definitions);

	UDataTable* Effects = NewObject<UDataTable>(ItemUse);
	Effects->RowStruct = FTDItemUseEffectRow::StaticStruct();
	FTDItemUseEffectRow Effect;
	Effect.ItemId = TEXT("Expand");
	Effect.EffectTag = TDTags::Item_Effect_ExpandInventory.GetTag();
	Effect.Value = 1.f;
	Effects->AddRow(TEXT("ExpandEffect"), Effect);
	FObjectPropertyBase* EffectTable = FindFProperty<FObjectPropertyBase>(ItemUse->GetClass(), TEXT("UseEffectTable"));
	if (!TestNotNull(TEXT("Use effect table property"), EffectTable)) return false;
	EffectTable->SetObjectPropertyValue_InContainer(ItemUse, Effects);

	TestEqual(TEXT("Exactly six slots"), Quick->GetSlots().Num(), 6);
	TestTrue(TEXT("Give consumable"), Inventory->AddItem(TEXT("Expand"), 2));
	TestTrue(TEXT("Give equipment"), Inventory->AddItem(TEXT("Equipment"), 1));
	Quick->ServerSetSlot(0, ETDQuickSlotType::Item, TEXT("Expand"));
	TestEqual(TEXT("Register owned consumable"), Quick->GetSlot(0).Id, FName(TEXT("Expand")));

	for (const FName InvalidId : {FName(TEXT("Missing")), FName(TEXT("Equipment")), FName(TEXT("NotOwned"))})
	{
		Quick->ServerSetSlot(0, ETDQuickSlotType::Item, InvalidId);
		TestEqual(TEXT("Rejected registration preserves existing slot"), Quick->GetSlot(0).Id, FName(TEXT("Expand")));
	}
	Quick->ServerSetSlot(5, ETDQuickSlotType::Item, TEXT("Expand"));
	TestTrue(TEXT("Duplicate registration clears old slot"), Quick->GetSlot(0).IsEmpty());
	TestEqual(TEXT("New slot has inventory quantity"), Quick->GetSlotItemCount(5), 2);
	TestTrue(TEXT("Move registered item in inventory"), Inventory->MoveItem(0, 7));
	const int32 Before = Inventory->GetSlotCapacity();
	Quick->ServerUseSlot(5);
	TestEqual(TEXT("Use still finds moved item by ID"), Quick->GetSlotItemCount(5), 1);
	TestEqual(TEXT("Use actually applies effect"), Inventory->GetSlotCapacity(), Before + 1);
	Quick->ServerUseSlot(5);
	TestEqual(TEXT("Last item consumed"), Quick->GetSlotItemCount(5), 0);
	TestFalse(TEXT("Depleted item remains registered"), Quick->GetSlot(5).IsEmpty());
	Quick->ServerUseSlot(5);
	TestEqual(TEXT("Empty inventory cannot apply effect"), Inventory->GetSlotCapacity(), Before + 2);
	TestTrue(TEXT("Reacquire registered item"), Inventory->AddItem(TEXT("Expand"), 1));
	TestEqual(TEXT("Reacquired item available in same slot"), Quick->GetSlotItemCount(5), 1);
	Quick->ServerClearSlot(5);
	TestTrue(TEXT("Clear registration"), Quick->GetSlot(5).IsEmpty());
	TestEqual(TEXT("Clear does not consume inventory"), Inventory->GetItemCount(TEXT("Expand")), 1);
	return true;
}
#endif
