#include "TDNavMenuWidget.h"

#include "Nav/TDNavMenuTypes.h"
#include "UI/HUD/Nav/TDNavMenuButtonWidget.h"

void UTDNavMenuWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	InitializeNavEntries();
}

void UTDNavMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeNavEntries();

	for (UTDNavMenuButtonWidget* Entry : NavEntries)
	{
		Entry->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEntryClicked);
	}

	SelectEntry(InventoryEntry);
}

void UTDNavMenuWidget::NativeDestruct()
{
	for (UTDNavMenuButtonWidget* Entry : NavEntries)
	{
		if (Entry)
		{
			Entry->OnClicked.RemoveDynamic(this, &ThisClass::HandleEntryClicked);
		}
	}

	Super::NativeDestruct();
}

void UTDNavMenuWidget::InitializeNavEntries()
{
	NavEntries.Reset();

	auto AddEntry = [this](UTDNavMenuButtonWidget* Entry, ETDNavMenuType MenuType)
	{
		if (!Entry)
		{
			return;
		}

		Entry->SetMenuType(MenuType);
		Entry->SetStyleData(ButtonStyleData);
		NavEntries.Add(Entry);
	};

	AddEntry(InventoryEntry, ETDNavMenuType::Inventory);
	AddEntry(QuestEntry, ETDNavMenuType::Quest);
	AddEntry(CharacterEntry, ETDNavMenuType::Character);
	AddEntry(SkillEntry, ETDNavMenuType::Skill);
	AddEntry(PartyEntry, ETDNavMenuType::Party);
	AddEntry(SystemEntry, ETDNavMenuType::System);
}

void UTDNavMenuWidget::SelectEntry(UTDNavMenuButtonWidget* NewSelectedEntry)
{
	if (!NewSelectedEntry)
	{
		return;
	}

	SelectedEntry = NewSelectedEntry;

	for (UTDNavMenuButtonWidget* Entry : NavEntries)
	{
		Entry->SetSelected(Entry == SelectedEntry);
	}
}

void UTDNavMenuWidget::HandleEntryClicked(
	UTDNavMenuButtonWidget* ButtonWidget,
	ETDNavMenuType MenuType
)
{
	SelectEntry(ButtonWidget);
	OnMenuRequested.Broadcast(MenuType);
}
