#include "UI/Layer/WindowLayer/Character/TDCharacterContentWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
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
}

void UTDCharacterContentWidget::NativeDestruct()
{
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
			F::PhysicalAttack, F::MagicalAttack, F::Defense, F::CriticalChance, F::HasPlayerState
		};
		for (const auto Field : Fields)
		{
			ViewModel->AddFieldValueChangedDelegate(Field,
				INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &ThisClass::OnFieldChanged));
		}
	}
	RefreshStats();
}

void UTDCharacterContentWidget::OnFieldChanged(UObject* Object, UE::FieldNotification::FFieldId Field)
{
	RefreshStats();
}

void UTDCharacterContentWidget::RefreshStats()
{
	const bool bReady = ViewModel && ViewModel->HasPlayerState;
	const FText Empty = FText::FromString(TEXT("-"));
	if (CharacterNameText) CharacterNameText->SetText(bReady ? ViewModel->PlayerName
		: NSLOCTEXT("TDCharacter", "Waiting", "캐릭터 대기 중"));
	if (LevelValueText) LevelValueText->SetText(bReady ? FText::AsNumber(ViewModel->Level) : Empty);
	if (HealthValueText) HealthValueText->SetText(bReady ? ViewModel->HealthText : Empty);
	if (ManaValueText) ManaValueText->SetText(bReady ? ViewModel->ManaText : Empty);
	if (AttackValueText)
	{
		AttackValueText->SetText(bReady ? FText::AsNumber(FMath::RoundToInt(ViewModel->PhysicalAttack)) : Empty);
		AttackValueText->SetToolTipText(bReady ? FText::Format(
			NSLOCTEXT("TDCharacter", "AttackDetails", "물리 공격력: {0}\n마법 공격력: {1}"),
			FText::AsNumber(FMath::RoundToInt(ViewModel->PhysicalAttack)),
			FText::AsNumber(FMath::RoundToInt(ViewModel->MagicalAttack))) : FText::GetEmpty());
	}
	if (DefenseValueText) DefenseValueText->SetText(bReady
		? FText::AsNumber(FMath::RoundToInt(ViewModel->Defense)) : Empty);
	if (CriticalValueText)
	{
		FNumberFormattingOptions Format;
		Format.MinimumFractionalDigits = 1;
		Format.MaximumFractionalDigits = 1;
		CriticalValueText->SetText(bReady ? FText::AsPercent(
			FMath::Clamp(ViewModel->CriticalChance, 0.f, 1.f), &Format) : Empty);
	}
}

void UTDCharacterContentWidget::SetCharacterPortrait(UTexture2D* InTexture)
{
	PortraitTexture = InTexture;
	if (CharacterPortrait)
	{
		CharacterPortrait->SetBrushFromTexture(InTexture);
		CharacterPortrait->SetVisibility(InTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UTDCharacterContentWidget::SetEquipmentVisual(ETDCharacterEquipmentSlot EquipmentSlot, const FTDItemSlotVisualData& Data)
{
	UTDItemSlotVisualWidget* Target = nullptr;
	switch (EquipmentSlot)
	{
	case ETDCharacterEquipmentSlot::Weapon: Target = WeaponSlot; break;
	case ETDCharacterEquipmentSlot::Necklace: Target = NecklaceSlot; break;
	case ETDCharacterEquipmentSlot::Ring: Target = RingSlot; break;
	case ETDCharacterEquipmentSlot::Crown: Target = CrownSlot; break;
	case ETDCharacterEquipmentSlot::Dress: Target = DressSlot; break;
	case ETDCharacterEquipmentSlot::Shoes: Target = ShoesSlot; break;
	}
	if (Target) Target->SetSlotVisualData(Data);
}
