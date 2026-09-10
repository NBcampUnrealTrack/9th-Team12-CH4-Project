#include "TDQuickSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "Skill/TDSkillComponent.h"
#include "UI/Common/Tooltip/TDItemTooltipWidget.h"
#include "Stats/TDProgressionComponent.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDItemRow.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Items/TDInventoryComponent.h"
#include "Items/TDQuickSlotComponent.h"
#include "Player/TDPlayerState.h"
#include "UI/Common/ItemSlot/TDItemDragDropOperation.h"
#include "UI/Common/ItemSlot/TDItemSlotVisualWidget.h"

void UTDQuickSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SlotWidgets.SetNum(UTDQuickSlotComponent::SlotCount);
	for (int32 Index = 0; Index < SlotWidgets.Num(); ++Index)
	{
		const FName Name(*FString::Printf(TEXT("QuickSlot_%d"), Index + 1));
		SlotWidgets[Index] = WidgetTree ? Cast<UTDItemSlotVisualWidget>(WidgetTree->FindWidget(Name)) : nullptr;
		if (!SlotWidgets[Index])
		{
			UE_LOG(LogTemp, Warning, TEXT("퀵슬롯: WBP_QuickSlot에 '%s' 이름의 슬롯베이스가 필요합니다."), *Name.ToString());
		}
	}
    // 기존 WBP의 구분선 다음 세 슬롯. 아이템 6칸과 별도로 표시만 관리한다.
    SkillWidgets.Reset();
    for (const FName Name : {FName(TEXT("QuickSlot")), FName(TEXT("QuickSlot_7")), FName(TEXT("QuickSlot_8"))})
    {
        SkillWidgets.Add(WidgetTree ? Cast<UTDItemSlotVisualWidget>(WidgetTree->FindWidget(Name)) : nullptr);
    }
    SkillSource.Reset();
    DisplayedSkillClass = NAME_None;
    RefreshSkillSlots();
	RefreshSources();
	RefreshSlots();
    RefreshSkillCooldowns();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SkillCooldownTimer, this, &ThisClass::RefreshSkillCooldowns, 0.1f, true);
		// PlayerState가 UI보다 늦게 도착하거나 교체되는 경우만 재연결한다.
		World->GetTimerManager().SetTimer(SourceCheckTimer, this, &ThisClass::RefreshSources, 0.25f, true);
	}
}

void UTDQuickSlotWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SourceCheckTimer);
        World->GetTimerManager().ClearTimer(SkillCooldownTimer);
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	UnbindSources();
	SlotWidgets.Reset();
    SkillWidgets.Reset();
    SkillSource.Reset();
    DisplayedSkillClass = NAME_None;
	PressedSlot = INDEX_NONE;
	PressedButton = FKey();
	Super::NativeDestruct();
}

void UTDQuickSlotWidget::UnbindSources()
{
	if (IsValid(QuickSlots)) QuickSlots->OnQuickSlotsChanged.RemoveDynamic(this, &ThisClass::QueueRefresh);
	if (IsValid(Inventory)) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::QueueRefresh);
	QuickSlots = nullptr;
	Inventory = nullptr;
}

void UTDQuickSlotWidget::RefreshSources()
{
	const APlayerController* Controller = GetOwningPlayer();
	const ATDPlayerState* State = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;
	UTDQuickSlotComponent* NewQuickSlots = State ? State->GetQuickSlotComponent() : nullptr;
	UTDInventoryComponent* NewInventory = State ? State->GetInventoryComponent() : nullptr;
    UTDProgressionComponent* Progression = State ? State->GetProgressionComponent() : nullptr;
    const FName ClassId = Progression ? Progression->GetClassId() : NAME_None;
    if (SkillSource.Get() != Progression || DisplayedSkillClass != ClassId)
    {
        SkillSource = Progression;
        DisplayedSkillClass = ClassId;
        RefreshSkillSlots();
    }
	if (QuickSlots == NewQuickSlots && Inventory == NewInventory) return;

	UnbindSources();
	QuickSlots = NewQuickSlots;
	Inventory = NewInventory;
	if (IsValid(QuickSlots)) QuickSlots->OnQuickSlotsChanged.AddUniqueDynamic(this, &ThisClass::QueueRefresh);
	if (IsValid(Inventory)) Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::QueueRefresh);
	RefreshSlots();
}

void UTDQuickSlotWidget::QueueRefresh()
{
	// FastArray의 삭제 알림은 항목이 지워지기 전에 온다. 다음 틱에 최종 수량을 읽는다.
	if (UWorld* World = GetWorld(); World && !World->GetTimerManager().IsTimerActive(RefreshTimer))
	{
		RefreshTimer = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RefreshSlots);
	}
}

void UTDQuickSlotWidget::RefreshSlots()
{
	for (int32 Index = 0; Index < SlotWidgets.Num(); ++Index)
	{
		UTDItemSlotVisualWidget* Visual = SlotWidgets[Index];
		if (!IsValid(Visual)) continue;
		const FTDQuickSlot SlotData = IsValid(QuickSlots) ? QuickSlots->GetSlot(Index) : FTDQuickSlot();
		const FTDItemRow* Definition = IsValid(Inventory) && SlotData.Type == ETDQuickSlotType::Item
			? Inventory->FindItemDefinition(SlotData.Id) : nullptr;
		Visual->SetSlotEnabled(true); // 빈 칸과 소진된 칸도 드롭과 등록 해제를 받아야 한다.
		Visual->SetRenderOpacity(1.f);
		if (!Definition)
		{
			Visual->ClearSlotVisual();
			Visual->SetToolTipText(NSLOCTEXT("TDQuickSlot", "Empty", "소비 아이템을 드래그해 등록하세요."));
			continue;
		}

		FTDItemSlotVisualData Data;
		Data.bHasItem = true;
		Data.DisplayName = Definition->DisplayName;
		Data.Icon = Definition->Icon;
		Data.Rarity = Definition->Rarity;
		Data.Count = Inventory->GetItemCount(SlotData.Id);
		Data.bAlwaysShowCount = true;
		Visual->SetSlotVisualData(Data);
		Visual->SetRenderOpacity(Data.Count > 0 ? 1.f : 0.45f);
		Visual->SetItemTooltipSource(SlotData.Id,
			NSLOCTEXT("TDQuickSlot", "TooltipHint", "좌클릭: 사용 / 우클릭: 등록 해제"));
	}
}

bool UTDQuickSlotWidget::RequestRegisterItem(int32 Index, FName ItemId)
{
	RefreshSources();
	if (!SlotWidgets.IsValidIndex(Index) || !IsValid(QuickSlots) || !IsValid(Inventory)) return false;
	const FTDItemRow* Definition = Inventory->FindItemDefinition(ItemId);
	if (!Definition || Definition->ItemType != TDTags::Item_Type_Consumable.GetTag()
		|| Inventory->GetItemCount(ItemId) <= 0) return false;
	QuickSlots->ServerSetSlot(Index, ETDQuickSlotType::Item, ItemId);
	return true;
}

void UTDQuickSlotWidget::RequestUseSlot(int32 Index)
{
	RefreshSources();
	if (SlotWidgets.IsValidIndex(Index) && IsValid(QuickSlots)) QuickSlots->ServerUseSlot(Index);
}

void UTDQuickSlotWidget::RequestClearSlot(int32 Index)
{
	RefreshSources();
	if (SlotWidgets.IsValidIndex(Index) && IsValid(QuickSlots)) QuickSlots->ServerClearSlot(Index);
}

int32 UTDQuickSlotWidget::FindSlotAt(const FVector2D& ScreenPosition) const
{
	for (int32 Index = 0; Index < SlotWidgets.Num(); ++Index)
	{
		const UTDItemSlotVisualWidget* Visual = SlotWidgets[Index];
		if (IsValid(Visual) && Visual->IsVisible() && Visual->GetCachedGeometry().IsUnderLocation(ScreenPosition)) return Index;
	}
	return INDEX_NONE;
}

FReply UTDQuickSlotWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const int32 Index = FindSlotAt(Event.GetScreenSpacePosition());
	const FKey Button = Event.GetEffectingButton();
	if (Index != INDEX_NONE && (Button == EKeys::LeftMouseButton || Button == EKeys::RightMouseButton))
	{
		PressedSlot = Index;
		PressedButton = Button;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

FReply UTDQuickSlotWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (PressedSlot != INDEX_NONE && Event.GetEffectingButton() == PressedButton)
	{
		const int32 Index = PressedSlot;
		const FKey Button = PressedButton;
		PressedSlot = INDEX_NONE;
		PressedButton = FKey();
		if (FindSlotAt(Event.GetScreenSpacePosition()) == Index)
		{
			if (Button == EKeys::LeftMouseButton) RequestUseSlot(Index);
			else RequestClearSlot(Index);
		}
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(Geometry, Event);
}

FReply UTDQuickSlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return NativeOnMouseButtonDown(Geometry, Event);
}

void UTDQuickSlotWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{
	PressedSlot = INDEX_NONE;
	PressedButton = FKey();
	Super::NativeOnMouseCaptureLost(Event);
}

bool UTDQuickSlotWidget::NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& Event, UDragDropOperation* Operation)
{
	const UTDItemDragDropOperation* ItemDrag = Cast<UTDItemDragDropOperation>(Operation);
	if (!ItemDrag) return Super::NativeOnDrop(Geometry, Event, Operation);
	RefreshSources();
	if (!IsValid(Inventory) || ItemDrag->SourceInventory.Get() != Inventory) return false;
	return RequestRegisterItem(FindSlotAt(Event.GetScreenSpacePosition()), ItemDrag->ItemId);
}

void UTDQuickSlotWidget::RefreshSkillSlots()
{
    const UTDProgressionComponent* Progression = SkillSource.Get();
    for (int32 Index = 0; Index < SkillWidgets.Num(); ++Index)
    {
        UTDItemSlotVisualWidget* Visual = SkillWidgets[Index];
        if (!Visual) continue;
        const FName SkillId = Progression ? Progression->GetSkillForSlot(Index + 1) : NAME_None;
        const FTDSkillRow* Row = Progression ? Progression->FindSkillRow(SkillId) : nullptr;
        UTextBlock* NameText = WidgetTree ? Cast<UTextBlock>(WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("SkillName_%d"), Index + 1)))) : nullptr;
        Visual->ClearSlotVisual();
        Visual->SetSlotEnabled(true);

        if (Row)
        {
            FTDItemSlotVisualData Data;
            Data.bHasItem = true;
            Data.DisplayName = Row->DisplayName;
            Data.Icon = Row->Icon;
            Visual->SetSlotVisualData(Data);
        }
        // 아이템 슬롯의 내부 시각 갱신과 별도로, 슬롯 전체 영역에 공용 카드를 연결한다.
        UWidget* TooltipHost = WidgetTree ? WidgetTree->FindWidget(
            FName(*FString::Printf(TEXT("QuickSlotKeyOverlay_%d"), Index + 7))) : nullptr;
        if (!TooltipHost) TooltipHost = Visual;
        TooltipHost->SetVisibility(ESlateVisibility::Visible);
        const TCHAR* Keys[] = {TEXT("Q"), TEXT("W"), TEXT("E")};
        const FText Details = Row ? FText::Format(
            NSLOCTEXT("TDQuickSlot", "SkillTooltip", "액티브 스킬 · {0}\n요구 레벨: {1}\n기본 마나: {2}\n기본 재사용 대기시간: {3}초"),
            FText::FromString(Keys[Index]), FText::AsNumber(Row->RequiredLevel),
            FText::AsNumber(Row->ManaCost), FText::AsNumber(Row->Cooldown)) : FText::GetEmpty();
        UTDItemTooltipWidget::AttachText(this, TooltipHost,
            Row ? Row->DisplayName : FText::GetEmpty(), Details);
        if (NameText)
        {
            NameText->SetText(Row ? Row->DisplayName : FText::GetEmpty());
            NameText->SetVisibility(Row && Row->Icon.IsNull()
                ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        }
    }
}

void UTDQuickSlotWidget::RefreshSkillCooldowns()
{
    const APlayerController* Controller = GetOwningPlayer();
    const APawn* CharacterPawn = Controller ? Controller->GetPawn() : nullptr;
    const UTDSkillComponent* Skills = CharacterPawn ? CharacterPawn->FindComponentByClass<UTDSkillComponent>() : nullptr;
    if (!WidgetTree) return;
    for (int32 SlotIndex = 1; SlotIndex <= 3; ++SlotIndex)
    {
        // 자체 타이머로 쿨을 추측하지 않고 실제 스킬 컴포넌트의 남은 시간을 읽는다.
        const float Remaining = Skills ? Skills->GetCooldownRemainingForSlot(SlotIndex) : 0.f;
        const bool bCoolingDown = Remaining > 0.f;
        const ESlateVisibility CooldownVisibility = bCoolingDown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
        if (UWidget* Shade = WidgetTree->FindWidget(FName(*FString::Printf(TEXT("SkillCooldownShade_%d"), SlotIndex))))
            Shade->SetVisibility(CooldownVisibility);
        if (UTextBlock* Text = Cast<UTextBlock>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("SkillCooldownText_%d"), SlotIndex)))))
        {
            Text->SetVisibility(CooldownVisibility);
            Text->SetText(bCoolingDown ? FText::AsNumber(FMath::CeilToInt(Remaining)) : FText::GetEmpty());
        }
    }
}
