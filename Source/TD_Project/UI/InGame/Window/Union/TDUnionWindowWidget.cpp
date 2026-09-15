#include "UI/InGame/Window/Union/TDUnionWindowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/TDCharacterClassRow.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Player/TDPlayerState.h"
#include "Settings/TDCharacterClassSettings.h"
#include "Stats/TDProgressionComponent.h"
#include "Stats/TDUnionBonus.h"
#include "TimerManager.h"
#include "UI/Common/Tooltip/TDTooltipStatics.h"
#include "UI/Common/Typography/TDTextBlock.h"

void UTDUnionWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 제목은 다른 창(퀘스트·강화·상점)처럼 코드에서 넣는다. Super 가 WindowTitle 을 먼저 입히므로 그 뒤에 덮는다.
	SetWindowTitle(NSLOCTEXT("TDUnionWindow", "WindowTitle", "유니온"));

	// 틀은 창과 연결되지 않아도 스스로 닫히고 끌려서(UTDWindowFrameWidget::HandleCloseClicked) 알아채기
	// 어렵다. 드러나는 증상은 제목이 "TEXTBLOCK" 으로 남는 것뿐인데, 실제로는 ✕ 가 틀만 지우고
	// 창은 목록에 남아 다음 단축키가 한 번 헛돈다. WBP 의 틀 이름이 달라 실제로 겪었다.
	if (GetWindowFrame() == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("유니온 창: WindowFrame 이 연결되지 않았다. WBP 의 창 틀 위젯 이름을 정확히 'WindowFrame' 으로 둘 것 — "
			     "제목이 들어가지 않고, 닫기 버튼이 창이 아니라 틀만 지운다."));
	}

	bBuilt = false;
	DisplayedBestLevels.Reset();

	Refresh();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::Refresh, 0.25f, true);
	}
}

void UTDUnionWindowWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}

	Super::NativeDestruct();
}

void UTDUnionWindowWidget::Refresh()
{
	const APlayerController* Controller = GetOwningPlayer();
	const ATDPlayerState* State = Controller ? Controller->GetPlayerState<ATDPlayerState>() : nullptr;
	if (State == nullptr)
	{
		return;
	}

	const UTDProgressionComponent* Progression = State->GetProgressionComponent();
	const int32 CurrentLevel = Progression != nullptr ? Progression->GetLevel() : 1;

	TMap<FName, int32> BestLevels =
		TDUnion::GetBestLevels(State->GetCharacterSlots(), State->GetSelectedSlotIndex(), CurrentLevel);

	// 그려진 것과 같으면 넘어간다. 0.25초마다 도는데 줄을 매번 새로 만들 이유가 없다.
	if (bBuilt && BestLevels.OrderIndependentCompareEqual(DisplayedBestLevels))
	{
		return;
	}

	DisplayedBestLevels = MoveTemp(BestLevels);
	bBuilt = true;

	Rebuild(*State, CurrentLevel);
}

void UTDUnionWindowWidget::Rebuild(const ATDPlayerState& State, int32 CurrentLevel)
{
	if (EntryList == nullptr)
	{
		return;
	}

	EntryList->ClearChildren();

	const UTDCharacterClassSettings* ClassSettings = UTDCharacterClassSettings::Get();
	const UDataTable* BonusTable = ClassSettings ? ClassSettings->UnionBonusTable.LoadSynchronous() : nullptr;
	const UDataTable* ClassTable = ClassSettings ? ClassSettings->ClassTable.LoadSynchronous() : nullptr;

	if (BonusTable == nullptr)
	{
		AddLine(MissingTableText, InactiveColor, EntryStyle, 0.f, 0.f);
		return;
	}

	const TArray<FTDUnionEntry> Entries = TDUnion::BuildEntries(
		BonusTable, State.GetCharacterSlots(), State.GetSelectedSlotIndex(), CurrentLevel);

	// 직업 순서는 DT_CharacterClass 를 따른다. 표에만 있는 직업은 그 뒤에 붙인다.
	TArray<FName> ClassOrder;
	if (ClassTable != nullptr)
	{
		ClassOrder = ClassTable->GetRowNames();
	}

	for (const FTDUnionEntry& Entry : Entries)
	{
		ClassOrder.AddUnique(Entry.ClassId);
	}

	bool bFirstGroup = true;

	for (const FName& ClassId : ClassOrder)
	{
		TArray<const FTDUnionEntry*> ClassEntries;
		for (const FTDUnionEntry& Entry : Entries)
		{
			if (Entry.ClassId == ClassId)
			{
				ClassEntries.Add(&Entry);
			}
		}

		if (ClassEntries.IsEmpty())
		{
			continue;
		}

		ClassEntries.Sort([](const FTDUnionEntry& A, const FTDUnionEntry& B)
		{
			return A.RequiredLevel < B.RequiredLevel;
		});

		FText ClassName = FText::FromName(ClassId);
		if (ClassTable != nullptr)
		{
			const FTDCharacterClassRow* ClassRow =
				ClassTable->FindRow<FTDCharacterClassRow>(ClassId, TEXT("UTDUnionWindowWidget"), false);

			if (ClassRow != nullptr && !ClassRow->DisplayName.IsEmpty())
			{
				ClassName = ClassRow->DisplayName;
			}
		}

		const int32* BestLevel = DisplayedBestLevels.Find(ClassId);

		FFormatNamedArguments HeaderArgs;
		HeaderArgs.Add(TEXT("Class"), ClassName);
		HeaderArgs.Add(TEXT("Level"), BestLevel != nullptr ? FText::AsNumber(*BestLevel) : FText::FromString(TEXT("-")));

		AddLine(FText::Format(HeaderFormat, HeaderArgs), HeaderColor, HeaderStyle,
			0.f, bFirstGroup ? 0.f : GroupSpacing);

		bFirstGroup = false;

		for (const FTDUnionEntry* Entry : ClassEntries)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Level"), FText::AsNumber(Entry->RequiredLevel));
			Args.Add(TEXT("Stat"), UTDTooltipStatics::FormatStatName(Entry->StatTag));
			Args.Add(TEXT("Value"), UTDTooltipStatics::FormatStatValue(Entry->StatTag, Entry->Op, Entry->Value));

			AddLine(FText::Format(EntryFormat, Args),
				Entry->bActive ? ActiveColor : InactiveColor, EntryStyle, EntryIndent, 0.f);
		}
	}
}

void UTDUnionWindowWidget::AddLine(const FText& Text, const FLinearColor& Color, ETDTextStyleRole Role,
	float Indent, float TopPadding)
{
	if (WidgetTree == nullptr || EntryList == nullptr)
	{
		return;
	}

	// 다른 창과 같은 폰트가 나오도록 프로젝트의 텍스트 위젯을 쓴다. 스타일 역할을 먼저 넣어야
	// 테마가 적용된 뒤에 색이 덮인다 — 순서가 바뀌면 테마가 색을 되돌린다.
	UTDTextBlock* Line = WidgetTree->ConstructWidget<UTDTextBlock>();
	Line->SetTextStyleRole(Role);
	Line->SetTypographyColorOverride(Color);
	Line->SetText(Text);

	if (UVerticalBoxSlot* LineSlot = EntryList->AddChildToVerticalBox(Line))
	{
		LineSlot->SetPadding(FMargin(Indent, TopPadding, 0.f, 2.f));
	}
}
