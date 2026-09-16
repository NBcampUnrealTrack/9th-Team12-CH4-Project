#include "Option/TDOptionServiceComponent.h"

#include "Character/TDPlayerCharacter.h"
#include "Core/TDGameplayTags.h"
#include "Data/TDItemRow.h"
#include "Data/TDOptionRarityRow.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/TDDialogueSource.h"
#include "Interaction/TDInteractionFlowComponent.h"
#include "Items/TDInventoryComponent.h"
#include "Items/TDItemOptionStatics.h"
#include "Items/TDItemUseComponent.h"
#include "Player/TDPlayerState.h"
#include "Settings/TDItemOptionSettings.h"
#include "UI/Core/TDUIManagerSubsystem.h"
#include "UI/Layer/WindowLayer/Option/TDOptionWindowWidget.h"
#include "UI/Settings/TDUISettings.h"
#include "World/TDNPCBase.h"

UTDOptionServiceComponent::UTDOptionServiceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

APlayerController* UTDOptionServiceComponent::GetController() const
{
	return Cast<APlayerController>(GetOwner());
}

UTDInventoryComponent* UTDOptionServiceComponent::GetInventory() const
{
	const APlayerController* Controller = GetController();

	const ATDPlayerState* State = Controller
		? Controller->GetPlayerState<ATDPlayerState>()
		: nullptr;

	return State ? State->GetInventoryComponent() : nullptr;
}

UTDItemUseComponent* UTDOptionServiceComponent::GetItemUse() const
{
	const APlayerController* Controller = GetController();

	const ATDPlayerState* State = Controller
		? Controller->GetPlayerState<ATDPlayerState>()
		: nullptr;

	return State ? State->GetItemUseComponent() : nullptr;
}

void UTDOptionServiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void UTDOptionServiceComponent::StartForNPC(ATDNPCBase* NPC)
{
	APlayerController* Controller = GetController();

	if (!Controller
		|| !Controller->HasAuthority()
		|| !IsValid(NPC)
		|| !NPC->IsOptionNPC()
		|| !GetWorld())
	{
		return;
	}

	EndService();

	ATDPlayerState* State = Controller->GetPlayerState<ATDPlayerState>();

	ActiveNPC = NPC;
	ActivePawn = Controller->GetPawn();
	ActiveInventory = GetInventory();
	StartZone = State ? State->GetCurrentZoneId() : FGameplayTag();

	SessionCounter = SessionCounter >= MAX_int32 ? 1 : SessionCounter + 1;

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

	// 대화 종료와 챕터 처리가 끝난 뒤 창을 열도록 다음 검사에서 처리합니다.
	GetWorld()->GetTimerManager().SetTimer(
		ServerTimer,
		this,
		&ThisClass::TickServer,
		0.1f,
		true);
}

bool UTDOptionServiceComponent::IsServerContextValid() const
{
	APlayerController* Controller = GetController();
	ATDNPCBase* NPC = ActiveNPC.Get();

	ATDPlayerCharacter* Character = Cast<ATDPlayerCharacter>(ActivePawn.Get());

	if (!Controller
		|| !Controller->HasAuthority()
		|| ActiveSessionId <= 0
		|| !IsValid(NPC)
		|| !IsValid(Character)
		|| Controller->GetPawn() != Character
		|| Character->IsDead()
		|| !NPC->IsOptionNPC()
		|| NPC->GetWorld() != Character->GetWorld()
		|| !ActiveInventory.IsValid()
		|| ActiveInventory.Get() != GetInventory())
	{
		return false;
	}

	const ATDPlayerState* State = Controller->GetPlayerState<ATDPlayerState>();

	if (!State || State->GetCurrentZoneId() != StartZone)
	{
		return false;
	}

	if (!ITDDialogueSource::Execute_IsDialogueSourceInRange(NPC, Character))
	{
		return false;
	}

	// NPC의 표시 조건과 대화 가능 여부도 기존 판정을 사용합니다.
	return NPC->CanInteract_Implementation(Character);
}

void UTDOptionServiceComponent::TickServer()
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

		ClientOpenService(MakeView(TEXT("추가 옵션을 다시 굴릴 장신구를 선택하세요."), 0, true));

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

FTDOptionWindowView UTDOptionServiceComponent::MakeView(
	const FString& Message,
	int32 ReplyRequestId,
	bool bResetSelection)
{
	FTDOptionWindowView View;
	View.SessionId = ActiveSessionId;
	View.Message = Message;
	View.ReplyRequestId = ReplyRequestId;
	View.bResetSelection = bResetSelection;

	UTDInventoryComponent* Inventory = ActiveInventory.Get();
	UTDItemUseComponent* ItemUse = GetItemUse();

	if (!Inventory || !ItemUse)
	{
		return View;
	}

	View.Gold = Inventory->GetGold();
	View.InventoryRevision = Inventory->GetInventoryRevision();

	// 등급표는 아이템마다 다시 읽을 이유가 없다. 한 번 정렬해 두고 조회만 한다.
	const UTDItemOptionSettings* Settings = UTDItemOptionSettings::Get();

	const TArray<FTDOptionRarityRow> Sorted = Settings != nullptr
		? TDItemOption::GetSortedRarities(Settings->OptionRarityTable.LoadSynchronous())
		: TArray<FTDOptionRarityRow>();

	for (const FTDItemInstance& Item : Inventory->GetItems())
	{
		const FTDItemRow* Definition = Inventory->FindItemDefinition(Item.ItemId);

		// 강화 창과 같은 기준이다 — 겹치는 물건은 개체를 특정할 수 없다.
		if (!Definition
			|| Definition->ItemType != TDTags::Item_Type_Accessory.GetTag()
			|| Definition->bStackable
			|| Item.Count != 1)
		{
			continue;
		}

		FTDOptionItemView Entry;
		Entry.SlotIndex = Item.SlotIndex;
		Entry.ItemId = Item.ItemId;
		Entry.DisplayName = Definition->DisplayName.IsEmpty()
			? FText::FromName(Item.ItemId)
			: Definition->DisplayName;
		Entry.Icon = Definition->Icon;
		Entry.Options = Item.Options;

		// 한 번도 안 굴린 것은 등급이 비어 있다. PrepareReroll 과 같은 규칙으로 메운다 —
		// 창에 "(없음)" 이 뜨면 플레이어는 굴릴 수 없는 아이템으로 읽는다.
		Entry.OptionRarity = Item.OptionRarity;
		if (!Entry.OptionRarity.IsValid())
		{
			Entry.OptionRarity = Definition->InitialOptionRarity.IsValid()
				? Definition->InitialOptionRarity
				: (Sorted.Num() > 0 ? Sorted[0].Rarity : FGameplayTag());
		}

		const FTDOptionRarityRow* RarityRow =
			TDItemOption::FindRarity(Sorted, Entry.OptionRarity);

		Entry.UpgradeChance = RarityRow != nullptr ? RarityRow->UpgradeChance : 0.f;

		// 최고 등급이면 더 오를 곳이 없다. 확률을 그대로 보여주면 거짓말이 된다.
		if (RarityRow != nullptr
			&& TDItemOption::GetNextRarity(Sorted, Entry.OptionRarity) == Entry.OptionRarity)
		{
			Entry.UpgradeChance = 0.f;
		}

		Entry.RerollCost = ItemUse->GetRerollCost(Item.SlotIndex, /*bEquipped=*/false);

		// 비용이 0 이면 풀·등급표 어느 쪽이 빠진 것이다. 굴려도 InternalError 가 된다.
		Entry.bCanReroll = !Definition->OptionPoolId.IsNone()
			&& RarityRow != nullptr
			&& Entry.RerollCost > 0;

		View.Items.Add(Entry);
	}

	View.Items.Sort(
		[](const FTDOptionItemView& A, const FTDOptionItemView& B)
		{
			return A.SlotIndex < B.SlotIndex;
		});

	LastSentRevision = View.InventoryRevision;
	LastSentGold = View.Gold;

	return View;
}

void UTDOptionServiceComponent::SendView(
	const FString& Message,
	int32 ReplyRequestId,
	bool bResetSelection)
{
	if (ActiveSessionId > 0 && bClientOpened)
	{
		ClientUpdateService(MakeView(Message, ReplyRequestId, bResetSelection));
	}
}

void UTDOptionServiceComponent::ServerTryReroll_Implementation(
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
		|| (Flow && (Flow->IsDialogueActive() || Flow->IsChapterPresentationActive())))
	{
		EndService();
		return;
	}

	// 같은 요청을 두 번 처리하지 않습니다.
	if (RequestId <= 0 || RequestId <= LastRequestId || bProcessingRequest)
	{
		return;
	}

	LastRequestId = RequestId;

	UTDInventoryComponent* Inventory = ActiveInventory.Get();
	UTDItemUseComponent* ItemUse = GetItemUse();

	if (!ItemUse)
	{
		SendView(TEXT("추가 옵션을 굴릴 수 없습니다."), RequestId, false);
		return;
	}

	if (ExpectedRevision != Inventory->GetInventoryRevision())
	{
		SendView(TEXT("가방이 변경되었습니다. 장신구를 다시 선택하세요."), RequestId, true);
		return;
	}

	bProcessingRequest = true;

	FGameplayTag NewRarity;

	// 굴리기·골드·확률은 전부 저쪽 규칙이다. 여기서 다시 판단하면 두 곳이 어긋난다.
	const ETDRerollResult Result =
		ItemUse->RerollOptionsForService(SlotIndex, /*bEquipped=*/false, NewRarity);

	bProcessingRequest = false;

	FString Message;

	switch (Result)
	{
	case ETDRerollResult::SuccessUpgraded:
		Message = TEXT("등급이 상승했습니다! 추가 옵션을 새로 굴렸습니다.");
		break;

	case ETDRerollResult::Success:
		Message = TEXT("추가 옵션을 새로 굴렸습니다.");
		break;

	case ETDRerollResult::NotEnoughGold:
		Message = TEXT("골드가 부족합니다.");
		break;

	case ETDRerollResult::ItemNotFound:
		Message = TEXT("선택한 장신구가 가방에 없습니다.");
		break;

	case ETDRerollResult::NotAccessory:
		Message = TEXT("추가 옵션은 장신구만 가질 수 있습니다.");
		break;

	default:
		Message = TEXT("추가 옵션을 굴릴 수 없습니다. 아이템과 옵션 데이터를 확인하세요.");
		break;
	}

	if (ActiveSessionId != SessionId)
	{
		return;
	}

	// 서버의 최신 골드와 아이템 정보를 결과와 함께 보냅니다.
	SendView(Message, RequestId, false);

	// 기존 결과 이벤트를 쓰는 코드(연출·사운드)와의 연결도 유지합니다.
	ItemUse->ClientOptionsRerolled(Result, NewRarity);
}

void UTDOptionServiceComponent::EndService()
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

void UTDOptionServiceComponent::ServerCloseService_Implementation(int32 SessionId)
{
	if (SessionId > 0 && SessionId == ActiveSessionId)
	{
		EndService();
	}
}

void UTDOptionServiceComponent::ClientOpenService_Implementation(const FTDOptionWindowView& View)
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

void UTDOptionServiceComponent::TryShowLocalWindow()
{
	APlayerController* Controller = GetController();

	if (!Controller || LocalView.SessionId <= 0 || LocalWindow)
	{
		return;
	}

	const UTDInteractionFlowComponent* Flow =
		Controller->FindComponentByClass<UTDInteractionFlowComponent>();

	if (Flow && (Flow->IsDialogueActive() || Flow->IsChapterPresentationActive()))
	{
		return;
	}

	ULocalPlayer* Player = Controller->GetLocalPlayer();

	UTDUIManagerSubsystem* Manager = Player
		? Player->GetSubsystem<UTDUIManagerSubsystem>()
		: nullptr;

	// 창 클래스는 프로젝트 설정에서 읽는다 — 에셋을 코드에 박지 않는 기존 방식(TD UI).
	// 설정 칸은 창 공통 타입이라, 엉뚱한 창을 지정했으면 여기서 걸러 낸다.
	UClass* LoadedClass = GetDefault<UTDUISettings>()->OptionWindowClass.LoadSynchronous();

	const TSubclassOf<UTDOptionWindowWidget> WindowClass =
		LoadedClass != nullptr && LoadedClass->IsChildOf(UTDOptionWindowWidget::StaticClass())
			? LoadedClass
			: nullptr;

	if (WindowClass == nullptr
		|| Manager == nullptr
		|| !Manager->IsGameplayWindowLayerReady())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("추가 옵션 창을 열 수 없습니다. "
			     "프로젝트 세팅 > Game > TD UI > Option Window Class 와 WBP_Root 를 확인하세요."));

		const int32 FailedSession = LocalView.SessionId;
		CloseLocalWindow();
		ServerCloseService(FailedSession);
		return;
	}

	LocalWindow = CreateWidget<UTDOptionWindowWidget>(Controller, WindowClass);

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

	LocalWindow->OnWindowClosed.AddUniqueDynamic(this, &ThisClass::HandleWindowClosed);

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

void UTDOptionServiceComponent::ClientUpdateService_Implementation(const FTDOptionWindowView& View)
{
	if (View.SessionId != LocalView.SessionId || LocalView.SessionId <= 0)
	{
		return;
	}

	LocalView = View;

	if (LocalWindow)
	{
		LocalWindow->ApplyView(View);
	}
}

void UTDOptionServiceComponent::ClientCloseService_Implementation(int32 SessionId)
{
	if (SessionId == LocalView.SessionId)
	{
		CloseLocalWindow();
	}
}

void UTDOptionServiceComponent::HandleWindowClosed(UTDWindowBaseWidget* ClosedWindow)
{
	if (ClosedWindow != LocalWindow.Get())
	{
		return;
	}

	const int32 ClosedSession = LocalView.SessionId;

	CloseLocalWindow();
	ServerCloseService(ClosedSession);
}

void UTDOptionServiceComponent::CloseLocalWindow(bool bRestoreInput)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LocalOpenTimer);
	}

	const bool bHadWindow = IsValid(LocalWindow);

	if (LocalWindow)
	{
		UTDOptionWindowWidget* ClosingWindow = LocalWindow.Get();

		LocalWindow = nullptr;

		ClosingWindow->OnWindowClosed.RemoveDynamic(this, &ThisClass::HandleWindowClosed);

		ClosingWindow->RemoveFromParent();
	}

	LocalView = FTDOptionWindowView();

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

	const bool bKeepCursor = bCursorBeforeWindow || Manager->HasVisibleGameWindow();

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
