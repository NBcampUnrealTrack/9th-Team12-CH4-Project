#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Items/TDItemTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Stats/TDStatTypes.h"
#include "UI/Common/Tooltip/TDItemTooltipWidget.h"
#include "TDTooltipStatics.generated.h"

class APlayerController;
class UTDProgressionComponent;
struct FTDItemRow;

/**
 * 테이블의 문구에 실제 수치를 채워 넣는다.
 *
 * 테이블에는 문장만 있고 숫자가 없다. 스킬 피해는 레벨마다 다르고 아이템 옵션은
 * 개체마다 다르므로, 문장에 숫자를 박으면 레벨 5 짜리 설명이 레벨 1 에도 그대로 보인다.
 *
 *   DT_Skill.Description             "공격력의 {Damage}% 피해를 준다."
 *   DT_OptionDefinition.DisplayName  "최대 체력 +{0}%"
 *
 * ── 왜 여기에 있는가 ──
 * UTDItemTooltipWidget 은 UI 담당의 것이고 아이템 정의 테이블만 읽는다.
 * 이 파일은 그 위젯이 그릴 수 있는 형태(FTDTooltipData)를 만들어 줄 뿐 위젯을 건드리지 않는다.
 * 위젯은 SetTooltipData 로 받기만 하면 된다.
 *
 * ── 서식 규칙 ──
 * 옵션은 값이 언제나 정확히 하나라 순서 인자 `{0}` 을 쓴다.
 * 스킬은 문장이 필요한 값을 문장이 고르므로(어떤 스킬은 지속시간을, 어떤 스킬은 피해를)
 * 순서로 정할 수 없다. 이름 인자를 쓴다 — 쓸 수 있는 이름은 FormatSkillDescription 에 적어 두었다.
 */
UCLASS()
class TD_PROJECT_API UTDTooltipStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 스탯 값 하나를 사람이 읽는 글자로.
	 *
	 * 비율인지 절대값인지는 두 가지가 정한다 — 연산이 Increased/More 이거나,
	 * DT_StatDefinition 에서 그 스탯이 bIsPercent 이거나. 둘 중 하나면 100 을 곱하고
	 * `%` 를 붙인다. 치명타 확률처럼 Added 지만 비율인 스탯이 있어서 연산만으로는 모자란다.
	 *
	 * 소수 자릿수도 DT_StatDefinition 에서 온다. UTDItemTooltipWidget 과 같은 규칙이라
	 * 아이템 툴팁과 스킬 툴팁의 같은 스탯이 다르게 보이지 않는다.
	 *
	 * @param bSigned  양수 앞에 `+` 를 붙일지. 옵션 목록은 붙이고, 문장 안에 들어갈 때는 뗀다.
	 * @param bWithPercentSign
	 *                 뒤에 `%` 를 붙일지. **문구 틀이 이미 `%` 를 갖고 있으면 꺼야 한다** —
	 *                 DT_OptionDefinition 의 "최대 체력 +{0}%" 가 그렇고, 켠 채로 넣으면 `%%` 가 된다.
	 *                 100 을 곱하는 것은 이 값과 무관하다. 숫자는 언제나 비율로 환산된다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Tooltip")
	static FText FormatStatValue(FGameplayTag StatTag, ETDModOp Op, float Value,
		bool bSigned = true, bool bWithPercentSign = true);

	/**
	 * 스탯의 표시 이름(DT_StatDefinition.DisplayName). 없으면 태그 문자열을 그대로 돌려준다.
	 *
	 * FormatStatValue 와 짝이다. 문구 틀 없이 "최대 체력 +3%" 처럼 이름과 값을 직접 늘어놓는
	 * 창(유니온)이 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Tooltip")
	static FText FormatStatName(FGameplayTag StatTag);

	// ── 아이템 추가 옵션 ──────────────────────────────────

	/**
	 * 굴려진 옵션 한 줄. "최대 체력 +{0}%" 에 0.12 를 넣어 "최대 체력 +12%" 가 된다.
	 *
	 * DT_OptionDefinition 은 UTDItemUseComponent 가 들고 있으므로 컨트롤러에서 찾아간다 —
	 * 같은 테이블 참조를 UI 가 따로 들면 한쪽만 지정해 놓고 왜 안 되는지 찾게 된다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Tooltip")
	static FText FormatItemOption(const APlayerController* Owner, const FTDItemOption& Option);

	/**
	 * 옵션 전부를 툴팁 줄로. FTDItemInstance::Options 를 그대로 넘기면 된다.
	 *
	 * 문구 전체가 Label 에 들어가고 Value 는 비어 있다. 옵션은 "이름 : 값" 이 아니라
	 * "최대 체력 +12%" 라는 한 문장이라 쪼갤 자리가 없다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Tooltip")
	static TArray<FTDTooltipLine> MakeOptionLines(const APlayerController* Owner,
		const TArray<FTDItemOption>& Options);

	// ── 아이템 고정 스탯 (강화 반영) ──────────────────────

	/**
	 * DT_ItemStat 의 스탯들을 줄로. **강화 배율이 곱해진 값**이다.
	 *
	 * `UTDItemUseComponent::RefreshEquipmentModifiers` 가 실제로 스탯을 등록할 때와
	 * 같은 식(값 × 배율)을 쓴다. 화면 숫자와 실제 적용값이 어긋나지 않게 하려는 것이다.
	 *
	 * 강화 단수가 0 보다 크면 올라간 만큼을 괄호로 덧붙인다.
	 *
	 *   +0    물리공격력   +10
	 *   +7    물리공격력   +14 (+4)
	 *
	 * @param EnhanceLevel  `FTDItemInstance::EnhanceLevel`. **+1 을 넣으면 미리보기**가 된다 —
	 *                      "다음 강화에 성공하면 얼마가 되는지" 를 같은 함수로 만든다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Tooltip")
	static TArray<FTDTooltipLine> MakeItemStatLines(const APlayerController* Owner,
		FName ItemId, int32 EnhanceLevel);

	/** 장신구 세트의 실제 착용 개수와 단계별 효과를 표시 데이터로만 변환한다. */
	static TArray<FTDTooltipLine> MakeItemSetLines(const FTDItemRow& Item,
		const UDataTable* SetTable, const UDataTable* BonusTable, int32 EquippedCount);

	// ── 스킬 ──────────────────────────────────────────────

	/**
	 * DT_Skill.Description 의 이름 인자를 채운다.
	 *
	 * 쓸 수 있는 이름 —
	 *
	 *   시전    {Level} {Mana} {Cooldown} {CastTime} {Duration} {Interval} {Ticks} {Range} {Width}
	 *   효과    {Damage} {Heal} {RestoreMana}          Skill.Effect.* 의 끝 조각
	 *   패시브  {Defense_Armor} 와 {Armor}             Stat. 을 뗀 나머지, 그리고 끝 조각
	 *
	 * 패시브의 끝 조각은 겹칠 수 있다(Health.Max 와 Mana.Max 가 둘 다 Max). 겹치면 먼저
	 * 등록된 쪽이 남으므로, 그런 스킬은 긴 이름을 쓸 것.
	 *
	 * ── `%` 는 문장에 쓰지 않는다 ──
	 * 효과와 패시브 인자는 **이미 완성된 값**이다. 배율이면 `%` 가 붙어서 나온다.
	 *
	 *   "공격력의 {Damage} 피해"    →  "공격력의 150% 피해"
	 *   "최대 체력이 {Max} 오른다"  →  "최대 체력이 3% 오른다"
	 *   "체력을 {Heal} 회복한다"    →  "체력을 80 회복한다"
	 *
	 * 문장에 `%` 를 또 쓰면 "150%%" 가 된다. 값이 비율인지 절대값인지는 연산과
	 * DT_StatDefinition 이 정하므로 시트 쪽에서 알 필요가 없다.
	 *
	 * 시전 인자는 반대다 — 초·cm 같은 물리 단위라 문장이 단위를 말한다("{Duration}초").
	 *
	 * 없는 이름을 쓰면 그 자리가 글자 그대로 화면에 남는다. Tools/ValidateData.py 가 잡는다.
	 *
	 * @param Level  0 이하면 1 로 본다. 아직 찍지 않은 스킬도 "1 을 찍으면 이렇게 된다" 를
	 *               보여줘야 찍을지 정할 수 있다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Tooltip")
	static FText FormatSkillDescription(const UTDProgressionComponent* Progression,
		FName SkillId, int32 Level);

	/**
	 * 스킬 하나를 툴팁 한 장으로.
	 *
	 * 줄(Lines)에는 시전 정보만 담는다 — 마나 소모, 재사용 대기, 사거리.
	 * 피해나 패시브 수치는 Description 이 이미 문장으로 말하고 있어 줄로 또 내면 겹친다.
	 *
	 * 재사용 대기는 테이블의 원본이다. 실제 쿨은 Stat.Utility.CooldownRecoveryRate 로
	 * 나뉘지만, 그 값을 쓰면 툴팁이 캐릭터마다 달라져 "이 스킬의 쿨" 을 말할 수 없게 된다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Tooltip")
	static FTDTooltipData MakeSkillTooltip(const UTDProgressionComponent* Progression,
		FName SkillId, int32 Level);
};
