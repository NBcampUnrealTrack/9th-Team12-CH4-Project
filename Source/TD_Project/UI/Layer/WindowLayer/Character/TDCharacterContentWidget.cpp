#include "UI/Layer/WindowLayer/Character/TDCharacterContentWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "UI/Common/Typography/TDTextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Player/TDPlayerState.h"
#include "Items/TDItemUseComponent.h"
#include "Items/TDInventoryComponent.h"
#include "Data/TDItemRow.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"
#include "UI/Layer/WindowLayer/Inventory/TDInventoryActionPolicy.h"
#include "UI/Common/Tooltip/TDItemTooltipWidget.h"
#include "UI/ViewModel/TDPlayerStatsSubsystem.h"
#include "UI/ViewModel/TDPlayerStatsViewModel.h"

void UTDCharacterContentWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	SetCharacterPortrait(PortraitTexture);
}

void UTDCharacterContentWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (BasicTabButton) BasicTabButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBasicTabClicked);
	if (DetailTabButton) DetailTabButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDetailTabClicked);
	SetActiveInfoTab(0);
	OnPresentationInitialized();
	if (ViewModel)
	{
		SetViewModel(ViewModel);
	}
	else if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UTDPlayerStatsSubsystem* Stats = LocalPlayer->GetSubsystem<UTDPlayerStatsSubsystem>())
		{
			Stats->RefreshSource();
			SetViewModel(Stats->GetPlayerStatsViewModel());
		}
	}
	RefreshStats();
	RefreshEquipment();
	if (UWorld* World = GetWorld(); World && World->IsGameWorld())
	{
		// 접속 직후 PlayerState가 늦게 오거나 캐릭터 전환으로 교체되면 다시 연결한다.
		World->GetTimerManager().SetTimer(EquipmentSourceTimer, this, &ThisClass::CheckEquipmentSource, 0.25f, true);
	}
}

void UTDCharacterContentWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EquipmentSourceTimer);
		World->GetTimerManager().ClearTimer(EquipmentRefreshTimer);
	}
	UnbindEquipmentSource();
	if (BasicTabButton) BasicTabButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleBasicTabClicked);
	if (DetailTabButton) DetailTabButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleDetailTabClicked);
	if (ViewModel) ViewModel->RemoveAllFieldValueChangedDelegates(this);
	Super::NativeDestruct();
}

void UTDCharacterContentWidget::SetViewModel(UTDPlayerStatsViewModel* InViewModel)
{
	if (ViewModel) ViewModel->RemoveAllFieldValueChangedDelegates(this);
	ViewModel = InViewModel;
	if (ViewModel)
	{
		using F = UTDPlayerStatsViewModel::FFieldNotificationClassDescriptor;
		const UE::FieldNotification::FFieldId Fields[] = {
			F::PlayerName, F::Level, F::HealthText, F::ManaText,
			F::MaxHealth, F::MaxMana, F::PhysicalAttack, F::MagicalAttack, F::Defense,
			F::CriticalChance, F::CriticalDamage, F::ArmorPenetration, F::BossDamage,
			F::DamageReduction, F::HealthRegen, F::ManaRegen, F::MoveSpeed,
			F::CooldownRecoveryRate, F::CombatPower, F::HasPlayerState, F::CharacterFullBody
		};
		for (const auto Field : Fields)
		{
			ViewModel->AddFieldValueChangedDelegate(Field,
				INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::OnFieldChanged));
		}
	}
	SetCharacterPortrait(ViewModel ? ViewModel->CharacterFullBody.Get() : nullptr);
	RefreshStats();
}

void UTDCharacterContentWidget::OnFieldChanged(UObject* Object, UE::FieldNotification::FFieldId Field)
{
	if (Field == UTDPlayerStatsViewModel::FFieldNotificationClassDescriptor::CharacterFullBody)
		SetCharacterPortrait(ViewModel ? ViewModel->CharacterFullBody.Get() : nullptr);
	RefreshStats();
}

void UTDCharacterContentWidget::RefreshStats()
{
 OnStatsPresentation(ViewModel, ViewModel && ViewModel->HasPlayerState);
}

void UTDCharacterContentWidget::SetActiveInfoTab(int32 TabIndex)
{
 ActiveInfoTab = FMath::Clamp(TabIndex, 0, 1);
 OnInfoTabChanged(ActiveInfoTab);
}

int32 UTDCharacterContentWidget::GetActiveInfoTab() const
{
 return ActiveInfoTab;
}

void UTDCharacterContentWidget::ConfigureTextTooltip(FName HostName, const FText& Title, const FText& Description)
{
 if (WidgetTree)
  if (UWidget* Host = WidgetTree->FindWidget(HostName))
   UTDItemTooltipWidget::AttachText(this, Host, Title, Description);
}
void UTDCharacterContentWidget::HandleBasicTabClicked()
{
	SetActiveInfoTab(0);
}

void UTDCharacterContentWidget::HandleDetailTabClicked()
{
	SetActiveInfoTab(1);
}

void UTDCharacterContentWidget::SetCharacterPortrait(UTexture2D* InTexture)
{
	PortraitTexture = InTexture;
	OnPortraitChanged(InTexture, IsValid(InTexture));
}

void UTDCharacterContentWidget::SetEquipmentVisual(int32 EquipmentSlotIndex, const FTDItemSlotVisualData& Data)
{
	UTDItemSlotVisualWidget* EquipmentSlots[] = {Slot1, Slot2, Slot3, Slot4, Slot5, Slot6};
	if (EquipmentSlotIndex < 0 || EquipmentSlotIndex >= UE_ARRAY_COUNT(EquipmentSlots))
	{
		UE_LOG(LogTemp, Warning, TEXT("캐릭터 정보: 장착 칸 번호는 0~5여야 합니다. (입력: %d)"), EquipmentSlotIndex);
		return;
	}
	if (UTDItemSlotVisualWidget* Target = EquipmentSlots[EquipmentSlotIndex])
	{
		Target->SetSlotVisualData(Data);
	}
}

void UTDCharacterContentWidget::UnbindEquipmentSource()
{
	if (IsValid(EquipmentSource))
		EquipmentSource->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::QueueEquipmentRefresh);
	EquipmentSource = nullptr;
	EquipmentInventory = nullptr;
}

void UTDCharacterContentWidget::CheckEquipmentSource()
{
	UpdateUnequipPending();
	const APlayerController* Controller = GetOwningPlayer();
	const ATDPlayerState* State = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;
	UTDItemUseComponent* Source = State ? State->GetItemUseComponent() : nullptr;
	UTDInventoryComponent* Inventory = State ? State->GetInventoryComponent() : nullptr;
	if (EquipmentSource != Source || EquipmentInventory != Inventory) RefreshEquipment();
}

void UTDCharacterContentWidget::QueueEquipmentRefresh()
{
	// FastArray 삭제 알림은 실제 제거 전에 온다. 다음 틱에 최종 장착 목록을 읽는다.
	if (UWorld* World = GetWorld(); World && !World->GetTimerManager().IsTimerActive(EquipmentRefreshTimer))
		EquipmentRefreshTimer = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RefreshEquipment);
}

void UTDCharacterContentWidget::RefreshEquipment()
{
	if (IsDesignTime() || !GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) return;
	const APlayerController* Controller = GetOwningPlayer();
	const ATDPlayerState* State = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;
	UTDItemUseComponent* Source = State ? State->GetItemUseComponent() : nullptr;
	UTDInventoryComponent* Inventory = State ? State->GetInventoryComponent() : nullptr;
	if (EquipmentSource != Source || EquipmentInventory != Inventory)
	{
		UnbindEquipmentSource();
		EquipmentSource = Source;
		EquipmentInventory = Inventory;
		if (IsValid(EquipmentSource))
			EquipmentSource->OnEquipmentChanged.AddUniqueDynamic(this, &ThisClass::QueueEquipmentRefresh);
	}
	UTDItemSlotVisualWidget* Slots[] = {Slot1, Slot2, Slot3, Slot4, Slot5, Slot6};
	static_assert(UE_ARRAY_COUNT(Slots) == UTDItemUseComponent::EquipSlotCount);
	DisplayedEquipment.SetNum(UE_ARRAY_COUNT(Slots));
	UpdateUnequipPending();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Slots); ++Index)
{
		UTDItemSlotVisualWidget* Visual = Slots[Index];
		DisplayedEquipment[Index] = FTDItemInstance();
		if (!IsValid(Visual)) continue;
		// 배열 순서가 아니라 장착 칸 번호로 조회한다.
		const FTDItemInstance* Item = IsValid(EquipmentSource) ? EquipmentSource->GetEquipped(Index) : nullptr;
		const FTDItemRow* Definition = Item && Item->IsValid() && IsValid(EquipmentInventory)
			? EquipmentInventory->FindItemDefinition(Item->ItemId) : nullptr;
		if (!Definition)
		{
			Visual->ClearSlotVisual();
			continue;
		}
		FTDItemSlotVisualData Data;
		DisplayedEquipment[Index] = *Item;
		Data.bHasItem = true;
		Data.DisplayName = Definition->DisplayName;
		Data.Icon = Definition->Icon;
		Data.Rarity = Definition->Rarity;
		Data.Count = Item->Count;
		// 교체 전 아이템 ID로 툴팁이 잠깐 갱신되지 않도록 소스부터 교체한다.
		Visual->SetItemTooltipSource(NAME_None, FText::GetEmpty());
		Visual->SetSlotVisualData(Data);
		Visual->SetItemTooltipSource(Item->ItemId, EquippedActionHint);
	}
}

void UTDCharacterContentWidget::UpdateUnequipPending()
{
	if (!bUnequipPending) return;
	const APlayerController* Controller = GetOwningPlayer();
	const ATDPlayerState* State = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;
	const UTDItemUseComponent* CurrentSource = State ? State->GetItemUseComponent() : nullptr;
	if (!PendingUnequipSource.IsValid() || CurrentSource != PendingUnequipSource.Get()
		|| !TDInventoryAction::SameItem(CurrentSource->GetEquipped(PendingUnequipItem.SlotIndex), PendingUnequipItem)
		|| FPlatformTime::Seconds() - UnequipRequestedAt >= 3.0)
	{
		// 기존 RPC에는 실패 응답이 없으므로 타임아웃은 잠금만 해제하고 재전송하지 않는다.
		bUnequipPending = false;
		PendingUnequipSource.Reset();
	}
}

bool UTDCharacterContentWidget::RequestUnequipEquipment(int32 EquipmentSlotIndex)
{
	if (IsDesignTime() || !GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer
		|| !DisplayedEquipment.IsValidIndex(EquipmentSlotIndex)) return false;
	APlayerController* Controller = GetOwningPlayer();
	const ATDPlayerState* State = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;
	if (!Controller || !Controller->IsLocalController() || !State) return false;
	UTDItemUseComponent* Source = State->GetItemUseComponent();
	UTDInventoryComponent* Inventory = State->GetInventoryComponent();
	if (!IsValid(Source) || !IsValid(Inventory) || EquipmentSource != Source || EquipmentInventory != Inventory)
	{
		RefreshEquipment();
		return false;
	}
	UpdateUnequipPending();
	const double Now = FPlatformTime::Seconds();
	if (bUnequipPending || Now - UnequipRequestedAt < 0.2) return false;
	const FTDItemInstance* Equipped = Source->GetEquipped(EquipmentSlotIndex);
	if (!Equipped || !Equipped->IsValid()
		|| !TDInventoryAction::SameItem(Equipped, DisplayedEquipment[EquipmentSlotIndex])) return false;
	// 서버도 같은 검사를 한다. 빈 칸이 없으면 아이콘을 먼저 지우거나 요청을 보내지 않는다.
	if (Inventory->GetUsedSlotCount() >= Inventory->GetSlotCapacity())
	{
		UE_LOG(LogTemp, Log, TEXT("장착 해제: 인벤토리에 빈 칸이 없습니다."));
		return false;
	}
	PendingUnequipItem = *Equipped;
	PendingUnequipSource = Source;
	UnequipRequestedAt = Now;
	bUnequipPending = true; // Listen Server의 즉시 실행보다 먼저 저장한다.
	Source->ServerUnequipItem(EquipmentSlotIndex);
	return true;
}

FReply UTDCharacterContentWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::RightMouseButton)
	{
		UTDItemSlotVisualWidget* Slots[] = {Slot1, Slot2, Slot3, Slot4, Slot5, Slot6};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Slots); ++Index)
		{
			UTDItemSlotVisualWidget* EquipmentVisual = Slots[Index];
			if (IsValid(EquipmentVisual) && EquipmentVisual->IsVisible() && EquipmentVisual->GetIsEnabled()
				&& EquipmentVisual->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition()))
			{
				RequestUnequipEquipment(Index);
				return FReply::Handled();
			}
		}
	}
	return Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
}
