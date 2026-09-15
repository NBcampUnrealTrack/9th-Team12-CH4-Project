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

	SelectEntry(nullptr);
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
	AddEntry(UnionEntry, ETDNavMenuType::Union);
}

void UTDNavMenuWidget::SelectEntry(UTDNavMenuButtonWidget* NewSelectedEntry)
{
	SelectedEntry = NewSelectedEntry;

	for (UTDNavMenuButtonWidget* Entry : NavEntries)
	{
		Entry->SetSelected(Entry == SelectedEntry);
	}
}

void UTDNavMenuWidget::SetMenuSelected(
	ETDNavMenuType MenuType,
	bool bSelected)
{
	UTDNavMenuButtonWidget* TargetEntry = nullptr;
	switch (MenuType)
	{
	case ETDNavMenuType::Inventory:
		TargetEntry = InventoryEntry;
		break;
	case ETDNavMenuType::Quest:
		TargetEntry = QuestEntry;
		break;
	case ETDNavMenuType::Character:
		TargetEntry = CharacterEntry;
		break;
	case ETDNavMenuType::Skill:
		TargetEntry = SkillEntry;
		break;
	case ETDNavMenuType::Party:
		TargetEntry = PartyEntry;
		break;
	case ETDNavMenuType::System:
		TargetEntry = SystemEntry;
		break;
	case ETDNavMenuType::Union:
		TargetEntry = UnionEntry;
		break;
	default:
		break;
	}

	if (!TargetEntry)
	{
		return;
	}

	if (bSelected)
	{
		SelectEntry(TargetEntry);
	}
	else if (SelectedEntry == TargetEntry)
	{
		SelectEntry(nullptr);
	}
	else
	{
		TargetEntry->SetSelected(false);
	}
}

void UTDNavMenuWidget::HandleEntryClicked(
	UTDNavMenuButtonWidget* ButtonWidget,
	ETDNavMenuType MenuType
)
{
	(void)ButtonWidget;
	OnMenuRequested.Broadcast(MenuType);
}
