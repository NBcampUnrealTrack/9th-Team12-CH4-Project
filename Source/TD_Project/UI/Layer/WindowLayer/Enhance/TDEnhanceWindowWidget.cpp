#include "UI/Layer/WindowLayer/Enhance/TDEnhanceWindowWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Enhance/TDEnhanceServiceComponent.h"
#include "UI/Layer/WindowLayer/Enhance/TDEnhanceItemEntryWidget.h"

void UTDEnhanceWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetWindowTitle(FText::FromString(TEXT("장신구 강화")));

	if (BTN_Enhance)
	{
		BTN_Enhance->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleEnhanceClicked);
	}

	RefreshWindow();
}

void UTDEnhanceWindowWidget::NativeDestruct()
{
	if (BTN_Enhance)
	{
		BTN_Enhance->OnClicked.RemoveDynamic(
			this,
			&ThisClass::HandleEnhanceClicked);
	}

	Super::NativeDestruct();
}

void UTDEnhanceWindowWidget::InitializeService(
	UTDEnhanceServiceComponent* InService)
{
	Service = InService;
}

void UTDEnhanceWindowWidget::ApplyView(
	const FTDEnhanceWindowView& InView)
{
	if (InView.bResetSelection)
	{
		SelectedSlot = INDEX_NONE;
	}

	if (PendingRequestId > 0
		&& InView.ReplyRequestId == PendingRequestId)
	{
		PendingRequestId = 0;
	}

	CurrentView = InView;

	if (!FindSelectedItem())
	{
		SelectedSlot = INDEX_NONE;
	}

	RefreshWindow();

	if (TXT_Result && !InView.Message.IsEmpty())
	{
		TXT_Result->SetText(FText::FromString(InView.Message));
	}
}

const FTDEnhanceItemView*
UTDEnhanceWindowWidget::FindSelectedItem() const
{
	for (const FTDEnhanceItemView& Item : CurrentView.Items)
	{
		if (Item.SlotIndex == SelectedSlot)
		{
			return &Item;
		}
	}

	return nullptr;
}

void UTDEnhanceWindowWidget::RefreshWindow()
{
	if (TXT_Gold)
	{
		TXT_Gold->SetText(
			FText::Format(
				FText::FromString(TEXT("보유 골드: {0} G")),
				FText::AsNumber(CurrentView.Gold)));
	}

	if (TXT_Empty)
	{
		TXT_Empty->SetText(
			FText::FromString(
				TEXT("가방에 강화할 장신구가 없습니다.\n장착 중인 장신구는 해제 후 이용해 주세요.")));

		TXT_Empty->SetVisibility(
			CurrentView.Items.IsEmpty()
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}

	RefreshList();
	RefreshDetail();
}

void UTDEnhanceWindowWidget::RefreshList()
{
	if (!VB_Items)
	{
		return;
	}

	VB_Items->ClearChildren();

	if (!ItemEntryClass)
	{
		if (TXT_Result)
		{
			TXT_Result->SetText(
				FText::FromString(
					TEXT("Item Entry Class에 WBP_EnhanceItemEntry를 지정하세요.")));
		}
		return;
	}

	for (const FTDEnhanceItemView& Item : CurrentView.Items)
	{
		UTDEnhanceItemEntryWidget* Entry =
			CreateWidget<UTDEnhanceItemEntryWidget>(
				GetOwningPlayer(),
				ItemEntryClass);

		if (!Entry)
		{
			continue;
		}

		Entry->SetupEntry(
			Item,
			Item.SlotIndex == SelectedSlot);

		Entry->OnSelected.AddUniqueDynamic(
			this,
			&ThisClass::HandleItemSelected);

		VB_Items->AddChildToVerticalBox(Entry);
	}
}

void UTDEnhanceWindowWidget::RefreshDetail()
{
	if (!TXT_ItemName
		|| !TXT_Level
		|| !TXT_Cost
		|| !TXT_SuccessRate
		|| !TXT_FailureRule
		|| !BTN_Enhance
		|| !IMG_SelectedItem)
	{
		return;
	}

	BTN_Enhance->SetIsEnabled(false);

	const FTDEnhanceItemView* Item = FindSelectedItem();

	if (!Item)
	{
		IMG_SelectedItem->SetRenderOpacity(0.0f);
		TXT_ItemName->SetText(
			FText::FromString(TEXT("장신구를 선택하세요.")));
		TXT_Level->SetText(FText::GetEmpty());
		TXT_Cost->SetText(FText::GetEmpty());
		TXT_SuccessRate->SetText(FText::GetEmpty());
		TXT_FailureRule->SetText(FText::GetEmpty());
		return;
	}

	UTexture2D* Texture = Item->Icon.LoadSynchronous();

	IMG_SelectedItem->SetBrushFromTexture(Texture, false);
	IMG_SelectedItem->SetRenderOpacity(Texture ? 1.0f : 0.0f);
	TXT_ItemName->SetText(Item->DisplayName);

	if (!Item->bHasNextStep)
	{
		TXT_Level->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("현재 강화: +%d"),
					Item->EnhanceLevel)));

		TXT_Cost->SetText(
			FText::FromString(TEXT("다음 강화 단계 정보가 없습니다.")));

		TXT_SuccessRate->SetText(FText::GetEmpty());
		TXT_FailureRule->SetText(
			FText::FromString(
				TEXT("최대 단계인지, DT_Enhance 연결이 올바른지 확인하세요.")));
		return;
	}

	const FTDEnhanceRow& Rule = Item->NextStep;

	TXT_Level->SetText(
		FText::FromString(
			FString::Printf(
				TEXT("강화 단계: +%d → +%d"),
				Item->EnhanceLevel,
				Item->EnhanceLevel + 1)));

	TXT_Cost->SetText(
		FText::Format(
			FText::FromString(TEXT("필요 골드: {0} G")),
			FText::AsNumber(Rule.Cost)));

	TXT_SuccessRate->SetText(
		FText::FromString(
			FString::Printf(
				TEXT("성공 확률: %.1f%%"),
				Rule.SuccessRate * 100.0f)));

	FString FailureText;

	if (Rule.DowngradeChanceOnFail <= 0.0f
		|| Rule.MaxDowngradeTiers <= 0)
	{
		FailureText =
			TEXT("실패 시 강화 단계가 유지됩니다.");
	}
	else
	{
		FailureText = FString::Printf(
			TEXT("실패했을 때 %.1f%% 확률로 %d~%d단계 하락합니다."),
			Rule.DowngradeChanceOnFail * 100.0f,
			Rule.MinDowngradeTiers,
			Rule.MaxDowngradeTiers);
	}

	FailureText += TEXT("\n실패해도 골드는 소모됩니다.");

	if (CurrentView.Gold < Rule.Cost)
	{
		FailureText += TEXT("\n골드가 부족합니다.");
	}

	TXT_FailureRule->SetText(
		FText::FromString(FailureText));

	const bool bCanEnhance =
		Service.IsValid()
		&& CurrentView.SessionId > 0
		&& PendingRequestId == 0
		&& Rule.Cost >= 0
		&& CurrentView.Gold >= Rule.Cost;

	BTN_Enhance->SetIsEnabled(bCanEnhance);
}

void UTDEnhanceWindowWidget::HandleItemSelected(int32 SlotIndex)
{
	if (PendingRequestId != 0)
	{
		return;
	}

	SelectedSlot = SlotIndex;
	RefreshWindow();
}

void UTDEnhanceWindowWidget::HandleEnhanceClicked()
{
	if (!Service.IsValid()
		|| PendingRequestId != 0
		|| !FindSelectedItem()
		|| RequestCounter >= MAX_int32)
	{
		return;
	}

	++RequestCounter;

	// 리슨 서버에서는 답변이 즉시 올 수 있으므로 먼저 대기 상태로 만듭니다.
	PendingRequestId = RequestCounter;

	RefreshDetail();

	if (TXT_Result)
	{
		TXT_Result->SetText(
			FText::FromString(TEXT("강화 결과를 확인하고 있습니다...")));
	}

	Service->ServerTryEnhance(
		CurrentView.SessionId,
		CurrentView.InventoryRevision,
		SelectedSlot,
		PendingRequestId);
}