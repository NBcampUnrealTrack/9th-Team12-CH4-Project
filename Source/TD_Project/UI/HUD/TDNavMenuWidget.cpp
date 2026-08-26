#include "TDNavMenuWidget.h"

#include "Components/Button.h"
#include "UI/Common/TDHudNavButtonDA.h"

void UTDNavMenuWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	RefreshButtonStyles();
}

void UTDNavMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (InventoryButton)
	{
		InventoryButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleInventoryClicked
		);
	}

	if (QuestButton)
	{
		QuestButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleQuestClicked
		);
	}

	if (SystemButton)
	{
		SystemButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleSystemClicked
		);
	}

	// 처음 선택된 버튼
	SelectButton(InventoryButton);
}

void UTDNavMenuWidget::NativeDestruct()
{
	if (InventoryButton)
	{
		InventoryButton->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleInventoryClicked
		);
	}

	if (QuestButton)
	{
		QuestButton->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleQuestClicked
		);
	}

	if (SystemButton)
	{
		SystemButton->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleSystemClicked
		);
	}

	Super::NativeDestruct();
}

void UTDNavMenuWidget::SelectButton(UButton* NewSelectedButton)
{
	if (!ButtonStyleData || !NewSelectedButton)
	{
		return;
	}

	SelectedButton = NewSelectedButton;
	RefreshButtonStyles();
}

void UTDNavMenuWidget::RefreshButtonStyles()
{
	if (!ButtonStyleData)
	{
		return;
	}

	if (InventoryButton)
	{
		InventoryButton->SetStyle(
			ButtonStyleData->GetStyle(InventoryButton == SelectedButton)
		);
	}

	if (QuestButton)
	{
		QuestButton->SetStyle(
			ButtonStyleData->GetStyle(QuestButton == SelectedButton)
		);
	}

	if (SystemButton)
	{
		SystemButton->SetStyle(
			ButtonStyleData->GetStyle(SystemButton == SelectedButton)
		);
	}
}

void UTDNavMenuWidget::HandleInventoryClicked()
{
	SelectButton(InventoryButton);

	// 인벤토리 창 열기
}

void UTDNavMenuWidget::HandleQuestClicked()
{
	SelectButton(QuestButton);

	// 퀘스트 창 열기
}

void UTDNavMenuWidget::HandleSystemClicked()
{
	SelectButton(SystemButton);

	// 시스템 메뉴 열기
}