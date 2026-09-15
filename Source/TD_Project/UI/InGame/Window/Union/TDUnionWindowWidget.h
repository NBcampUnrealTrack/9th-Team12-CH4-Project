#pragma once

#include "CoreMinimal.h"
#include "UI/Common/Typography/TDTypographyThemeDA.h"
#include "UI/Layer/WindowLayer/Common/WindowBase/TDWindowBaseWidget.h"
#include "TDUnionWindowWidget.generated.h"

class ATDPlayerState;
class UVerticalBox;

/**
 * 유니온 창. 직업을 몇 레벨까지 키우면 계정 전체가 무엇을 받는지를 직업별로 늘어놓고,
 * 켜진 줄은 색으로 알린다.
 *
 *   전사              최고 Lv.45
 *     Lv.30   최대 체력 +3%        ← 켜짐(초록)
 *     Lv.50   최대 체력 +4%        ← 꺼짐(회색)
 *
 * WBP 에는 WindowFrame 과 빈 세로 상자 EntryList 만 두면 된다 — 줄은 여기서 만든다.
 * 스킬 창(UTDSkillWindowWidget)과 같은 방식이다.
 *
 * **판정은 서버와 같은 함수(TDUnion::BuildEntries)를 쓴다.** 따로 계산하면 창에는 켜졌다고
 * 나오는데 스탯은 안 오른 상태가 생긴다.
 */
UCLASS(Abstract, Blueprintable)
class TD_PROJECT_API UTDUnionWindowWidget : public UTDWindowBaseWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 줄이 들어갈 자리. 줄이 창을 넘치면 WBP 에서 ScrollBox 로 감싸면 된다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UVerticalBox> EntryList;

	// ── 문구 ──────────────────────────────────────────────

	/** 직업 제목줄. {Class} 직업 이름, {Level} 그 직업의 최고 레벨(캐릭터가 없으면 -). */
	UPROPERTY(EditAnywhere, Category = "TD|Union|Text")
	FText HeaderFormat = NSLOCTEXT("TDUnionWindow", "Header", "{Class}    최고 Lv.{Level}");

	/** 효과 줄. {Level} 필요 레벨, {Stat} 스탯 이름, {Value} 수치. */
	UPROPERTY(EditAnywhere, Category = "TD|Union|Text")
	FText EntryFormat = NSLOCTEXT("TDUnionWindow", "Entry", "Lv.{Level}    {Stat} {Value}");

	/** 테이블이 지정되지 않았을 때 창에 대신 띄우는 문구. */
	UPROPERTY(EditAnywhere, Category = "TD|Union|Text")
	FText MissingTableText = NSLOCTEXT("TDUnionWindow", "Missing", "유니온 정보를 불러오지 못했습니다.");

	// ── 색 ────────────────────────────────────────────────
	// 켜짐은 세트 효과 툴팁(MakeItemSetLines)과 같은 **초록 계열**을 쓴다. 창마다 "켜짐" 색이 다르면
	// 같은 뜻이 다르게 읽힌다. 다만 툴팁은 어두운 배경이고 이 창은 밝은 노란 배경이라, 툴팁의
	// 밝은 초록을 그대로 쓰면 눈이 부시고 글자가 번진다 — 계열은 두고 명도만 낮췄다.

	/**
	 * 직업 제목줄. 검은색이다 — 툴팁 제목의 금색을 쓰면 같은 계열인 창 틀 색에 묻혀 읽히지 않았다.
	 *
	 * 굵게 하지 않은 이유: UTDTextBlock(UI 담당)에는 굵기·글꼴 덮어쓰기가 없고, 테마 스타일을
	 * 다시 입힐 때 색·크기만 유지한다. 창 쪽에서 글꼴을 바꿔도 되돌아가므로 UI 담당 코드를
	 * 고치지 않는 한 확실히 굵게 만들 방법이 없다.
	 */
	UPROPERTY(EditAnywhere, Category = "TD|Union|Color")
	FLinearColor HeaderColor = FLinearColor::Black;

	/** 켜진 줄. 밝은 배경 위에서 읽히는 어두운 초록. FColor 로 적어 에디터 색상표의 값과 같게 했다. */
	UPROPERTY(EditAnywhere, Category = "TD|Union|Color")
	FLinearColor ActiveColor = FLinearColor(FColor(24, 110, 48));

	UPROPERTY(EditAnywhere, Category = "TD|Union|Color")
	FLinearColor InactiveColor = FLinearColor(0.55f, 0.58f, 0.63f);

	// ── 배치 ──────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category = "TD|Union|Style")
	ETDTextStyleRole HeaderStyle = ETDTextStyleRole::H4;

	UPROPERTY(EditAnywhere, Category = "TD|Union|Style")
	ETDTextStyleRole EntryStyle = ETDTextStyleRole::Body;

	/** 효과 줄을 제목줄보다 얼마나 들여 쓸지. */
	UPROPERTY(EditAnywhere, Category = "TD|Union|Style", meta = (ClampMin = "0"))
	float EntryIndent = 16.f;

	/** 직업 묶음 사이 간격. 첫 묶음 위에는 넣지 않는다. */
	UPROPERTY(EditAnywhere, Category = "TD|Union|Style", meta = (ClampMin = "0"))
	float GroupSpacing = 10.f;

private:
	/**
	 * 짧은 주기로 불린다. 직업별 최고 레벨이 달라졌을 때만 다시 그린다.
	 *
	 * 이벤트를 구독하지 않고 주기로 보는 이유는 값이 세 곳(캐릭터 목록 · 지금 슬롯 번호 ·
	 * 실시간 레벨)에서 따로 복제되어 도착 순서가 정해져 있지 않기 때문이다. 스킬 창도 같다.
	 */
	UFUNCTION()
	void Refresh();

	void Rebuild(const ATDPlayerState& State, int32 CurrentLevel);

	void AddLine(const FText& Text, const FLinearColor& Color, ETDTextStyleRole Role,
		float Indent, float TopPadding);

	/** 지금 그려져 있는 기준. 이것과 같으면 다시 그리지 않는다. */
	TMap<FName, int32> DisplayedBestLevels;
	bool bBuilt = false;

	FTimerHandle RefreshTimer;
};
