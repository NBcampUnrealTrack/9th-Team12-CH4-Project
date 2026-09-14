#include "Enhance/TDEnhanceServiceComponent.h"

#include "Character/TDPlayerCharacter.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDItemRow.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TDDialogueSource.h"
#include "Interaction/TDInteractionFlowComponent.h"
#include "Items/TDInventoryComponent.h"
#include "Player/TDPlayerState.h"
#include "UI/Core/TDUIManagerSubsystem.h"
#include "UI/Layer/WindowLayer/Enhance/TDEnhanceWindowWidget.h"
#include "World/TDNPCBase.h"

UTDEnhanceServiceComponent::UTDEnhanceServiceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

APlayerController*
UTDEnhanceServiceComponent::GetController() const
{
	return Cast<APlayerController>(GetOwner());
}

UTDInventoryComponent*
UTDEnhanceServiceComponent::GetInventory() const
{
	const APlayerController* Controller = GetController();

	const ATDPlayerState* State = Controller
		? Controller->GetPlayerState<ATDPlayerState>()
		: nullptr;

	return State ? State->GetInventoryComponent() : nullptr;
}

void UTDEnhanceServiceComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ServerTimer);
		GetWorld()->GetTimerManager().ClearTimer(LocalOpenTimer);
	}

	ActiveSessionId = 0;
	ActiveNPC.Reset();
	ActivePawn.Reset();
	ActiveInventory.Reset();

	CloseLocalWindow(false);

	Super::EndPlay(EndPlayReason);
}

void UTDEnhanceServiceComponent::StartForNPC(ATDNPCBase* NPC)
{
	APlayerController* Controller = GetController();

	if (!Controller
		|| !Controller->HasAuthority()
		|| !IsValid(NPC)
		|| !NPC->IsEnhanceNPC()
		|| !GetWorld())
	{
		return;
	}

	EndService();

	ATDPlayerState* State =
		Controller->GetPlayerState<ATDPlayerState>();

	ActiveNPC = NPC;
	ActivePawn = Controller->GetPawn();
	ActiveInventory = GetInventory();
	StartZone = State ? State->GetCurrentZoneId() : FGameplayTag();

	SessionCounter =
		SessionCounter >= MAX_int32 ? 1 : SessionCounter + 1;

	ActiveSessionId = SessionCounter;
	LastRequestId = 0;
	LastSentRevision = -1;
	LastSentGold = -1;
	bClientOpened = false;
	bProcessingRequest = false;

	if (!IsServerContextValid())
	{
		EndService();
		return;
	}

	// 大화 종료와 챕터 처리가 끝난 뒤 창을 열도록 다음 검사에서 처리합니다.
	GetWorld()->GetTimerManager().SetTimer(
		ServerTimer,
		this,
		&ThisClass::TickServer,
		0.1f,
		true);
}

bool UTDEnhanceServiceComponent::IsServerContextValid() const
{
	APlayerController* Controller = GetController();
	ATDNPCBase* NPC = ActiveNPC.Get();

	ATDPlayerCharacter* Character =
		Cast<ATDPlayerCharacter>(ActivePawn.Get());

	if (!Controller
		|| !Controller->HasAuthority()
		|| ActiveSessionId <= 0
		|| !IsValid(NPC)
		|| !IsValid(Character)
		|| Controller->GetPawn() != Character
		|| Character->IsDead()
		|| !NPC->IsEnhanceNPC()
		|| NPC->GetWorld() != Character->GetWorld()
		|| !ActiveInventory.IsValid()
		|| ActiveInventory.Get() != GetInventory())
	{
		return false;
	}

	const ATDPlayerState* State =
		Controller->GetPlayerState<ATDPlayerState>();

	if (!State || State->GetCurrentZoneId() != StartZone)
	{
		return false;
	}

	if (!ITDDialogueSource::Execute_IsDialogueSourceInRange(
		NPC,
		Character))
	{
		return false;
	}

	// NPC의 표시 조건과 대화 가능 여부도 기존 판정을 사용합니다.
	return NPC->CanInteract_Implementation(Character);
}

void UTDEnhanceServiceComponent::TickServer()
{
	if (!IsServerContextValid())
	{
		EndService();
		return;
	}

	APlayerController* Controller = GetController();

	const UTDInteractionFlowComponent* Flow =
		Controller->FindComponentByClass<UTDInteractionFlowComponent>();

	if (Flow && Flow->IsDialogueActive())
	{
		EndService();
		return;
	}

	if (Flow && Flow->IsChapterPresentationActive())
	{
		// 아직 열지 않았다면 챕터가 끝날 때까지 기다립니다.
		if (bClientOpened)
		{
			EndService();
		}
		return;
	}

	UTDInventoryComponent* Inventory = ActiveInventory.Get();

	if (!bClientOpened)
	{
		bClientOpened = true;

		ClientOpenService(
			MakeView(
				TEXT("강화할 장신구를 선택하세요."),
				0,
				true));

		return;
	}

	if (Inventory->GetInventoryRevision() != LastSentRevision
		|| Inventory->GetGold() != LastSentGold)
	{
		const bool bInventoryChanged =
			Inventory->GetInventoryRevision() != LastSentRevision;

		SendView(
			bInventoryChanged
				? TEXT("가방이 변경되었습니다. 장신구를 다시 선택하세요.")
				: FString(),
			0,
			bInventoryChanged);
	}
}

FTDEnhanceWindowView UTDEnhanceServiceComponent::MakeView(
	const FString& Message,
	int32 ReplyRequestId,
	bool bResetSelection)
{
	FTDEnhanceWindowView View;
	View.SessionId = ActiveSessionId;
	View.Message = Message;
	View.ReplyRequestId = ReplyRequestId;
	View.bResetSelection = bResetSelection;

	UTDInventoryComponent* Inventory = ActiveInventory.Get();

	if (!Inventory)
	{
		return View;
	}

	View.Gold = Inventory->GetGold();
	View.InventoryRevision = Inventory->GetInventoryRevision();

	for (const FTDItemInstance& Item : Inventory->GetItems())
	{
		const FTDItemRow* Definition =
			Inventory->FindItemDefinition(Item.ItemId);

		if (!Definition
			|| Definition->ItemType
				!= TDTags::Item_Type_Accessory.GetTag()
			|| Definition->bStackable
			|| Item.Count != 1)
		{
			continue;
		}

		FTDEnhanceItemView Entry;
		Entry.SlotIndex = Item.SlotIndex;
		Entry.ItemId = Item.ItemId;
		Entry.DisplayName = Definition->DisplayName.IsEmpty()
			? FText::FromName(Item.ItemId)
			: Definition->DisplayName;
		Entry.Icon = Definition->Icon;
		Entry.EnhanceLevel = Item.EnhanceLevel;

		if (Item.EnhanceLevel >= 0 && Item.EnhanceLevel < MAX_int32)
		{
			Entry.bHasNextStep = Inventory->GetEnhanceInfo(
				Item.EnhanceLevel + 1,
				Entry.NextStep);
		}

		View.Items.Add(Entry);
	}

	View.Items.Sort(
		[](const FTDEnhanceItemView& A, const FTDEnhanceItemView& B)
		{
			return A.SlotIndex < B.SlotIndex;
		});

	LastSentRevision = View.InventoryRevision;
	LastSentGold = View.Gold;

	return View;
}

void UTDEnhanceServiceComponent::SendView(
	const FString& Message,
	int32 ReplyRequestId,
	bool bResetSelection)
{
	if (ActiveSessionId > 0 && bClientOpened)
	{
		ClientUpdateService(
			MakeView(Message, ReplyRequestId, bResetSelection));
	}
}

void UTDEnhanceServiceComponent::ServerTryEnhance_Implementation(
	int32 SessionId,
	int64 ExpectedRevision,
	int32 SlotIndex,
	int32 RequestId)
{
	if (SessionId <= 0 || SessionId != ActiveSessionId)
	{
		return;
	}

	if (!IsServerContextValid())
	{
		EndService();
		return;
	}

	APlayerController* Controller = GetController();

	const UTDInteractionFlowComponent* Flow =
		Controller->FindComponentByClass<UTDInteractionFlowComponent>();

	if (!bClientOpened
		|| (Flow && (Flow->IsDialogueActive()
			|| Flow->IsChapterPresentationActive())))
	{
		EndService();
		return;
	}

	// 같은 요청을 두 번 처리하지 않습니다.
	if (RequestId <= 0
		|| RequestId <= LastRequestId
		|| bProcessingRequest)
	{
		return;
	}

	LastRequestId = RequestId;

	UTDInventoryComponent* Inventory = ActiveInventory.Get();

	if (ExpectedRevision != Inventory->GetInventoryRevision())
	{
		SendView(
			TEXT("가방이 변경되었습니다. 장신구를 다시 선택하세요."),
			RequestId,
			true);
		return;
	}

	bProcessingRequest = true;

	int32 NewLevel = 0;

	const ETDEnhanceResult Result =
		Inventory->EnhanceItemForService(SlotIndex, NewLevel);

	bProcessingRequest = false;

	FString Message;

	switch (Result)
	{
	case ETDEnhanceResult::Success:
		Message = FString::Printf(
			TEXT("강화 성공! 현재 강화 단계: +%d"),
			NewLevel);
		break;

	case ETDEnhanceResult::Downgraded:
		Message = FString::Printf(
			TEXT("강화 실패. 강화 단계가 +%d로 하락했습니다."),
			NewLevel);
		break;

	case ETDEnhanceResult::FailedNoChange:
		Message = FString::Printf(
			TEXT("강화 실패. +%d 단계가 유지되었습니다."),
			NewLevel);
		break;

	case ETDEnhanceResult::NotEnoughGold:
		Message = TEXT("골드가 부족합니다.");
		break;

	case ETDEnhanceResult::MaxLevelReached:
		Message = TEXT("다음 강화 단계가 없습니다.");
		break;

	case ETDEnhanceResult::ItemNotFound:
		Message = TEXT("선택한 장신구가 가방에 없습니다.");
		break;

	default:
		Message = TEXT("강화할 수 없습니다. 아이템과 강화 데이터를 확인하세요.");
		break;
	}

	if (ActiveSessionId != SessionId)
	{
		return;
	}

	// 서버의 최신 골드와 아이템 정보를 결과와 함께 보냅니다.
	SendView(Message, RequestId, false);

	// 기존 결과 이벤트를 사용하는 코드와의 연결도 유지합니다.
	Inventory->ClientItemEnhanced(SlotIndex, Result, NewLevel);
}

void UTDEnhanceServiceComponent::EndService()
{
	APlayerController* Controller = GetController();

	if (!Controller || !Controller->HasAuthority())
	{
		return;
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ServerTimer);
	}

	const int32 EndedSession = ActiveSessionId;

	ActiveSessionId = 0;
	ActiveNPC.Reset();
	ActivePawn.Reset();
	ActiveInventory.Reset();
	StartZone = FGameplayTag();
	bClientOpened = false;

	if (EndedSession > 0)
	{
		ClientCloseService(EndedSession);
	}
}

void UTDEnhanceServiceComponent::ServerCloseService_Implementation(
	int32 SessionId)
{
	if (SessionId > 0 && SessionId == ActiveSessionId)
	{
		EndService();
	}
}

void UTDEnhanceServiceComponent::ClientOpenService_Implementation(
	const FTDEnhanceWindowView& View)
{
	APlayerController* Controller = GetController();

	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}

	CloseLocalWindow();

	LocalView = View;

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			LocalOpenTimer,
			this,
			&ThisClass::TryShowLocalWindow,
			0.1f,
			true);
	}

	TryShowLocalWindow();
}

void UTDEnhanceServiceComponent::TryShowLocalWindow()
{
	APlayerController* Controller = GetController();

	if (!Controller || LocalView.SessionId <= 0 || LocalWindow)
	{
		return;
	}

	const UTDInteractionFlowComponent* Flow =
		Controller->FindComponentByClass<UTDInteractionFlowComponent>();

	if (Flow && (Flow->IsDialogueActive()
		|| Flow->IsChapterPresentationActive()))
	{
		return;
	}

	ULocalPlayer* Player = Controller->GetLocalPlayer();

	UTDUIManagerSubsystem* Manager = Player
		? Player->GetSubsystem<UTDUIManagerSubsystem>()
		: nullptr;

	if (EnhanceWindowClass == nullptr
		|| Manager == nullptr
		|| !Manager->IsGameplayWindowLayerReady())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("강화창을 열 수 없습니다. EnhanceWindowClass와 WBP_Root를 확인하세요."));

		const int32 FailedSession = LocalView.SessionId;
		CloseLocalWindow();
		ServerCloseService(FailedSession);
		return;
	}

	LocalWindow = CreateWidget<UTDEnhanceWindowWidget>(
		Controller,
		EnhanceWindowClass);

	if (!LocalWindow)
	{
		const int32 FailedSession = LocalView.SessionId;
		CloseLocalWindow();
		ServerCloseService(FailedSession);
		return;
	}

	LocalWindow->InitializeService(this);

	if (!Manager->ShowServiceWindow(LocalWindow))
	{
		const int32 FailedSession = LocalView.SessionId;
		CloseLocalWindow(false);
		ServerCloseService(FailedSession);
		return;
	}

	LocalWindow->OnWindowClosed.AddUniqueDynamic(
		this,
		&ThisClass::HandleWindowClosed);

	LocalWindow->ApplyView(LocalView);

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LocalOpenTimer);
	}

	bCursorBeforeWindow = Controller->bShowMouseCursor;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);

	Controller->SetInputMode(InputMode);
	Controller->bShowMouseCursor = true;
}

void UTDEnhanceServiceComponent::ClientUpdateService_Implementation(
	const FTDEnhanceWindowView& View)
{
	if (View.SessionId != LocalView.SessionId
		|| LocalView.SessionId <= 0)
	{
		return;
	}

	LocalView = View;

	if (LocalWindow)
	{
		LocalWindow->ApplyView(View);
	}
}

void UTDEnhanceServiceComponent::ClientCloseService_Implementation(
	int32 SessionId)
{
	if (SessionId == LocalView.SessionId)
	{
		CloseLocalWindow();
	}
}

void UTDEnhanceServiceComponent::HandleWindowClosed(
	UTDWindowBaseWidget* ClosedWindow)
{
	if (ClosedWindow != LocalWindow.Get())
	{
		return;
	}

	const int32 ClosedSession = LocalView.SessionId;

	CloseLocalWindow();
	ServerCloseService(ClosedSession);
}

void UTDEnhanceServiceComponent::CloseLocalWindow(bool bRestoreInput)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LocalOpenTimer);
	}

	const bool bHadWindow = IsValid(LocalWindow);

	if (LocalWindow)
	{
		UTDEnhanceWindowWidget* ClosingWindow = LocalWindow.Get();

		LocalWindow = nullptr;

		ClosingWindow->OnWindowClosed.RemoveDynamic(
			this,
			&ThisClass::HandleWindowClosed);

		ClosingWindow->RemoveFromParent();
	}

	LocalView = FTDEnhanceWindowView();

	if (!bRestoreInput || !bHadWindow)
	{
		return;
	}

	APlayerController* Controller = GetController();

	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}

	ULocalPlayer* Player = Controller->GetLocalPlayer();

	UTDUIManagerSubsystem* Manager = Player
		? Player->GetSubsystem<UTDUIManagerSubsystem>()
		: nullptr;

	// 로그인/캐릭터 선택 화면 등의 입력 상태는 건드리지 않습니다.
	if (!Manager || !Manager->IsGameplayWindowLayerReady())
	{
		return;
	}

	const UTDInteractionFlowComponent* Flow =
		Controller->FindComponentByClass<UTDInteractionFlowComponent>();

	if (Flow && Flow->IsDialogueActive())
	{
		return;
	}

	const bool bKeepCursor =
		bCursorBeforeWindow || Manager->HasVisibleGameWindow();

	if (bKeepCursor)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		Controller->SetInputMode(InputMode);
	}
	else
	{
		Controller->SetInputMode(FInputModeGameOnly());
	}

	Controller->bShowMouseCursor = bKeepCursor;
}