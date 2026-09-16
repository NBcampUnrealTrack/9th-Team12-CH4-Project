#include "UI/Layer/WindowLayer/Option/TDOptionWindowWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Option/TDOptionServiceComponent.h"
#include "UI/Common/Tooltip/TDTooltipStatics.h"
#include "UI/Layer/WindowLayer/Option/TDOptionItemEntryWidget.h"

void UTDOptionWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetWindowTitle(FText::FromString(TEXT("추가 옵션 재설정")));

	if (BTN_Reroll)
	{
		BTN_Reroll->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRerollClicked);
	}

	RefreshWindow();
}

void UTDOptionWindowWidget::NativeDestruct()
{
	if (BTN_Reroll)
	{
		BTN_Reroll->OnClicked.RemoveDynamic(this, &ThisClass::HandleRerollClicked);
	}

	Super::NativeDestruct();
}

void UTDOptionWindowWidget::InitializeService(UTDOptionServiceComponent* InService)
{
	Service = InService;
}

void UTDOptionWindowWidget::ApplyView(const FTDOptionWindowView& InView)
{
	if (InView.bResetSelection)
	{
		SelectedSlot = INDEX_NONE;
	}

	if (PendingRequestId > 0 && InView.ReplyRequestId == PendingRequestId)
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

const FTDOptionItemView* UTDOptionWindowWidget::FindSelectedItem() const
{
	for (const FTDOptionItemView& Item : CurrentView.Items)
	{
		if (Item.SlotIndex == SelectedSlot)
		{
			return &Item;
		}
	}

	return nullptr;
}

void UTDOptionWindowWidget::RefreshWindow()
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
				TEXT("가방에 추가 옵션을 굴릴 장신구가 없습니다.\n장착 중인 장신구는 해제 후 이용해 주세요.")));

		TXT_Empty->SetVisibility(
			CurrentView.Items.IsEmpty()
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}

	RefreshList();
	RefreshDetail();
}

void UTDOptionWindowWidget::RefreshList()
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
				FText::FromString(TEXT("Item Entry Class에 WBP_OptionItemEntry를 지정하세요.")));
		}
		return;
	}

	for (const FTDOptionItemView& Item : CurrentView.Items)
	{
		UTDOptionItemEntryWidget* Entry =
			CreateWidget<UTDOptionItemEntryWidget>(GetOwningPlayer(), ItemEntryClass);

		if (!Entry)
		{
			continue;
		}

		Entry->SetupEntry(Item, Item.SlotIndex == SelectedSlot);

		Entry->OnSelected.AddUniqueDynamic(this, &ThisClass::HandleItemSelected);

		VB_Items->AddChildToVerticalBox(Entry);
	}
}

void UTDOptionWindowWidget::RefreshDetail()
{
	if (!TXT_ItemName
		|| !TXT_Rarity
		|| !TXT_CurrentOptions
		|| !TXT_Cost
		|| !TXT_UpgradeRule
		|| !BTN_Reroll
		|| !IMG_SelectedItem)
	{
		return;
	}

	BTN_Reroll->SetIsEnabled(false);

	const FTDOptionItemView* Item = FindSelectedItem();

	if (!Item)
	{
		IMG_SelectedItem->SetRenderOpacity(0.0f);
		TXT_ItemName->SetText(FText::FromString(TEXT("장신구를 선택하세요.")));
		TXT_Rarity->SetText(FText::GetEmpty());
		TXT_CurrentOptions->SetText(FText::GetEmpty());
		TXT_Cost->SetText(FText::GetEmpty());
		TXT_UpgradeRule->SetText(FText::GetEmpty());
		return;
	}

	UTexture2D* Texture = Item->Icon.LoadSynchronous();

	IMG_SelectedItem->SetBrushFromTexture(Texture, false);
	IMG_SelectedItem->SetRenderOpacity(Texture ? 1.0f : 0.0f);
	TXT_ItemName->SetText(Item->DisplayName);

	FLinearColor RarityColor = FLinearColor::White;
	const FText RarityText = UTDOptionItemEntryWidget::GetRarityText(Item->OptionRarity, RarityColor);

	TXT_Rarity->SetText(
		FText::Format(FText::FromString(TEXT("옵션 등급: {0}")), RarityText));
	TXT_Rarity->SetColorAndOpacity(FSlateColor(RarityColor));

	// 붙어 있는 줄은 툴팁과 같은 경로로 문장을 만든다 — 두 화면의 표기가 갈리지 않는다.
	const TArray<FTDTooltipLine> Lines =
		UTDTooltipStatics::MakeOptionLines(GetOwningPlayer(), Item->Options);

	if (Lines.IsEmpty())
	{
		TXT_CurrentOptions->SetText(
			FText::FromString(TEXT("아직 추가 옵션이 없습니다.")));
	}
	else
	{
		FString Joined;
		for (const FTDTooltipLine& Line : Lines)
		{
			if (!Joined.IsEmpty())
			{
				Joined += TEXT("\n");
			}
			Joined += Line.Label.ToString();
		}

		TXT_CurrentOptions->SetText(FText::FromString(Joined));
	}

	if (!Item->bCanReroll)
	{
		TXT_Cost->SetText(FText::FromString(TEXT("이 장신구는 추가 옵션을 굴릴 수 없습니다.")));
		TXT_UpgradeRule->SetText(
			FText::FromString(
				TEXT("DT_ItemDefinition 의 OptionPoolId 와 DT_OptionRarity 연결을 확인하세요.")));
		return;
	}

	TXT_Cost->SetText(
		FText::Format(
			FText::FromString(TEXT("필요 골드: {0} G")),
			FText::AsNumber(Item->RerollCost)));

	FString RuleText;

	if (Item->UpgradeChance > 0.f)
	{
		RuleText = FString::Printf(
			TEXT("%.1f%% 확률로 옵션 등급이 한 단계 오릅니다."),
			Item->UpgradeChance * 100.0f);
	}
	else
	{
		RuleText = TEXT("이미 최고 등급이라 더 오르지 않습니다.");
	}

	// 줄 하나만 남기는 기능은 없다. 굴리면 세 줄이 통째로 바뀐다는 것을 먼저 알려준다.
	RuleText += TEXT("\n굴리면 지금 옵션은 모두 사라지고 새로 뽑습니다.");

	if (CurrentView.Gold < Item->RerollCost)
	{
		RuleText += TEXT("\n골드가 부족합니다.");
	}

	TXT_UpgradeRule->SetText(FText::FromString(RuleText));

	const bool bCanReroll =
		Service.IsValid()
		&& CurrentView.SessionId > 0
		&& PendingRequestId == 0
		&& CurrentView.Gold >= Item->RerollCost;

	BTN_Reroll->SetIsEnabled(bCanReroll);
}

void UTDOptionWindowWidget::HandleItemSelected(int32 SlotIndex)
{
	if (PendingRequestId != 0)
	{
		return;
	}

	SelectedSlot = SlotIndex;
	RefreshWindow();
}

void UTDOptionWindowWidget::HandleRerollClicked()
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
		TXT_Result->SetText(FText::FromString(TEXT("결과를 확인하고 있습니다...")));
	}

	Service->ServerTryReroll(
		CurrentView.SessionId,
		CurrentView.InventoryRevision,
		SelectedSlot,
		PendingRequestId);
}
