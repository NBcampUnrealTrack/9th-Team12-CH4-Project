#include "TDQuickSlotWidget.h"

#include "Blueprint/WidgetTree.h"
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
	RefreshSources();
	RefreshSlots();
	if (UWorld* World = GetWorld())
	{
		// PlayerState가 UI보다 늦게 도착하거나 교체되는 경우만 재연결한다.
		World->GetTimerManager().SetTimer(SourceCheckTimer, this, &ThisClass::RefreshSources, 0.25f, true);
	}
}

void UTDQuickSlotWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SourceCheckTimer);
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	UnbindSources();
	SlotWidgets.Reset();
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
		Visual->SetToolTipText(FText::Format(
			NSLOCTEXT("TDQuickSlot", "Item", "{0} · {1}개\n좌클릭: 사용 / 우클릭: 등록 해제"),
			Data.DisplayName, FText::AsNumber(Data.Count)));
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
