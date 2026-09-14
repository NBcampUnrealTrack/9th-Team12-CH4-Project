#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Skill/TDSkillTypes.h"
#include "TDSkillRow.generated.h"

class UNiagaraSystem;
class UTexture2D;

/**
 * DT_Skill 의 행. 스킬 하나의 정적 정의다.
 *
 * 세 직업의 스킬 18 종이 **전부 이 한 장에** 들어간다. 직업별로 테이블을 나누면
 * 직업이 늘 때마다 테이블과 그것을 읽는 코드가 함께 늘어난다.
 *
 * "무엇이 일어나는가" 는 여기 없다. 스킬 하나가 효과를 여러 개 가질 수 있어서
 * (피해를 주면서 나를 회복하는 스킬) 행 안에 배열이 필요해지고, 그러면 CSV 로
 * 편집할 수 없게 된다. DT_ItemDefinition ↔ DT_ItemStat 과 같은 이유로 나눈다.
 *
 *   액티브의 효과   DT_SkillEffect
 *   패시브의 효과   DT_SkillPassive
 *
 * 식별자는 RowName 이다. `UTDProgressionComponent::SkillLevels` 의 SkillId 와
 * 같은 문자열이어야 한다.
 */
USTRUCT(BlueprintType)
struct FTDSkillRow : public FTableRowBase
{
	GENERATED_BODY()

	/**
	 * 이 스킬을 쓰는 직업. DT_CharacterClass 의 RowName 이자 DT_ClassGrowth 의 ClassId 다.
	 *
	 * RowName 을 "Warrior_Slash" 처럼 짓더라도 **코드는 이 열만 읽는다.** 이름에서
	 * 문자열을 잘라 직업을 얻으면 스킬 이름 하나 바꿀 때 조회가 조용히 깨진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FName ClassId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FText DisplayName;

	/**
	 * 스킬창·툴팁에 보이는 설명.
	 *
	 * 수치는 `{0}` 같은 서식 자리로 비워 두고 UI 가 채운다. 레벨마다 값이 달라지므로
	 * 문장에 숫자를 박으면 레벨 5 짜리 설명이 레벨 1 에도 그대로 보인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FText Description;

	/** 하드 참조로 두면 테이블을 여는 것만으로 모든 아이콘이 메모리에 올라온다(D5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	ETDSkillType SkillType = ETDSkillType::Active;

	/**
	 * 액티브가 놓이는 자리. 1·2·3 이 각각 Q·W·E 다. 패시브는 0 으로 둔다.
	 *
	 * 플레이어가 배치하는 값이 아니라 직업이 정하는 값이라 퀵슬롯과 다르다 —
	 * 그래서 세이브에 들어가지 않고 테이블에 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0", ClampMax = "3"))
	int32 SlotIndex = 0;

	// ── 습득 ──────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Unlock", meta = (ClampMin = "1"))
	int32 MaxLevel = 5;

	/**
	 * 찍을 수 있게 되는 캐릭터 레벨. 1 이면 처음부터 찍을 수 있다.
	 *
	 * 선행 스킬 조건은 두지 않는다(Q10). 조건이 캐릭터 레벨 하나뿐이라 스킬창이
	 * 목록으로 끝나고, 서버 검증도 "포인트가 있나 / 레벨이 되나" 두 줄이면 된다.
	 *
	 * "액티브에만 레벨 제한, 1번 스킬과 패시브는 처음부터" 는 **이 열을 채우는 규칙**이지
	 * 코드가 강제하는 것이 아니다. SkillType 으로 분기하면 나중에 "패시브 3번은 30레벨부터"
	 * 로 바꾸고 싶을 때 C++ 을 고쳐야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Unlock", meta = (ClampMin = "1"))
	int32 RequiredLevel = 1;

	// ── 시전 (액티브 전용) ────────────────────────────────
	// 패시브는 아래 값을 전부 무시한다.

	/** 스킬 레벨 L 의 실제 소모량 = ManaCost + ManaCostPerLevel × (L - 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast", meta = (ClampMin = "0"))
	float ManaCost = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast")
	float ManaCostPerLevel = 0.f;

	/**
	 * 스킬 레벨 1 의 쿨타임(초).
	 *
	 * 실제 쿨타임은 평타와 같은 규칙으로 Stat.Utility.CooldownRecoveryRate 로 한 번 더 나뉜다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast", meta = (ClampMin = "0"))
	float Cooldown = 1.f;

	/**
	 * 레벨 한 칸당 쿨타임 변화(초). 레벨 L 의 쿨 = Cooldown + CooldownPerLevel × (L - 1).
	 *
	 * **줄이려면 음수를 넣는다.** ManaCostPerLevel 과 부호 규칙이 같아서, 시트에서
	 * "레벨당 얼마씩 변하는가" 를 같은 방식으로 읽을 수 있다.
	 *
	 * 수치가 오르지 않는 스킬(전사의 방벽처럼 무적 시간이 고정인 것)에 레벨을 올릴
	 * 이유를 주기 위한 열이다. 0 으로 두면 레벨과 무관한 예전 동작 그대로다.
	 *
	 * 0 아래로는 내려가지 않는다 — 값을 잘못 넣어도 쿨이 사라지지는 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast")
	float CooldownPerLevel = 0.f;

	/** Skill.Shape.* — 어디를 때리는가. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast")
	FGameplayTag ShapeTag;

	/**
	 * 사거리. 몸 중심에서 범위 끝까지의 거리(cm)다. 모양에 따라 뜻이 갈린다.
	 *
	 *   Skill.Shape.ForwardBox    앞으로 몇 cm 까지 닿는가
	 *   Skill.Shape.SelfRadius    원의 반경
	 *   Skill.Shape.Self          쓰지 않는다
	 *
	 * 평타는 몸 앞 160cm 까지 닿는다(HitBoxForwardOffset 100 + Extent 60).
	 * 스킬은 평타보다 길어야 하므로 그보다 큰 값을 넣는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast", meta = (ClampMin = "0"))
	float Range = 0.f;

	/**
	 * 좌우 폭(cm). Skill.Shape.ForwardBox 에서만 쓴다.
	 *
	 * 반값이 아니라 **전체 폭**이다. 160 이면 좌우로 80씩 닿는다 — 시트에 적는 사람이
	 * 반값을 계산하지 않아도 되도록 한 것이다.
	 *
	 * 높이는 열로 두지 않는다. 2.5D 라 모두 같은 평면에 있어서 평타와 같은 값(반높이 80)이면
	 * 충분하다. 공중 판정이 필요해지면 그때 열을 추가한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast", meta = (ClampMin = "0"))
	float Width = 0.f;

	/** 즉발·캐스팅·정신집중 중 무엇인가. 아래 CastDelay 의 의미가 이 값에 따라 달라진다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast")
	ETDSkillCastType CastType = ETDSkillCastType::Instant;

	/**
	 * 발동까지의 지연(초). **CastType 에 따라 뜻이 다르다.**
	 *
	 *   Instant   모션 안에서 판정이 일어나는 시점. 기다리는 것이 아니라 타이밍이다
	 *   Cast      캐스팅 시간. 이 동안 바가 차오르고 취소할 수 있다
	 *   Channel   시전 준비 시간. 보통 0 이고, 이 뒤부터 반복이 시작된다
	 *
	 * 셋을 한 열로 둔 이유는 어차피 하나만 쓰이기 때문이다. 열을 나누면 스킬마다
	 * 두 칸이 비고, 어느 칸을 채워야 하는지 시트를 보고는 알 수 없다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast", meta = (ClampMin = "0"))
	float CastDelay = 0.3f;

	/**
	 * 정신집중이 지속되는 시간(초). Channel 이 아니면 쓰지 않는다.
	 *
	 * 판정 횟수는 `ChannelDuration / ChannelInterval` 이다. 횟수를 직접 적지 않는 이유는
	 * 지속시간을 늘렸을 때 간격이 저절로 따라오게 하기 위해서다 — 둘 다 적으면 어긋난다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast", meta = (ClampMin = "0"))
	float ChannelDuration = 0.f;

	/**
	 * 정신집중 중 판정이 일어나는 간격(초). Channel 이 아니면 쓰지 않는다.
	 *
	 * **첫 판정은 정신집중이 시작되는 순간 바로 들어간다.** 한 박자 기다렸다 들어가면
	 * 곧바로 취소했을 때 아무것도 나가지 않아 답답하다. 판정 하나가 Interval 만큼의
	 * 구간을 대표하는 셈이라, Duration 2.0 / Interval 0.4 면 0·0.4·0.8·1.2·1.6 에
	 * 다섯 번 들어가고 2.0 에 끝난다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast", meta = (ClampMin = "0"))
	float ChannelInterval = 0.f;

	/**
	 * 시전하는 동안의 이동 제약.
	 *
	 * 세 CastType 모두에 적용된다. Instant 에 Locked 를 주면 짧은 시전 경직이 되고,
	 * Channel 에 Free 를 주면 뛰어다니며 쓰는 스킬이 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast")
	ETDSkillCastMovement CastMovement = ETDSkillCastMovement::Free;

	/**
	 * 시전 상황 태그. 조건부 장비 옵션("화염 스킬 피해 +30%")이 여기서 평가된다.
	 *
	 * `UTDCombatStatics::ApplyDamage` 의 ContextTags 로 그대로 넘어간다 —
	 * 조건부 모디파이어를 캐시하지 않고 시전 시점에 평가하기로 한 것(D16)이 여기서 쓰인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast")
	FGameplayTagContainer ContextTags;

	/**
	 * 시전할 때 재생할 이펙트.
	 *
	 * 애니메이션은 지정하지 않는다 — 세 직업 모두 평타 애님을 그대로 쓰기로 했다.
	 * 스킬 전용 애님이 생기면 그때 열을 추가한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast")
	TSoftObjectPtr<UNiagaraSystem> VFX;

	/**
	 * 이펙트 에셋이 **원래 몇 cm 짜리로 만들어졌는가**. 0 이면 크기를 건드리지 않는다.
	 *
	 * 코드는 에셋의 원래 크기를 알 방법이 없다. 이 값을 적어 두면 Range / VFXBaseSize 로
	 * 키워서, 판정 범위와 이펙트가 같은 크기가 된다 — 기획이 Range 를 바꾸면 이펙트가
	 * 저절로 따라온다.
	 *
	 * 모양에 따라 무엇을 재는지가 다르다.
	 *   ForwardBox   이펙트의 길이 (앞으로 뻗는 거리)
	 *   SelfRadius   이펙트의 반경
	 *   Self         쓰지 않는다 (Range 가 0 이라 키울 기준이 없다)
	 *
	 * 균등하게 키운다. 상자의 폭(Width)까지 따로 늘리면 이펙트가 찌그러진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast", meta = (ClampMin = "0"))
	float VFXBaseSize = 0.f;

	/**
	 * ForwardBox 에서만 쓴다. 이펙트가 **몸에서 몇 cm 앞에서 시작하는가**.
	 *
	 * 앞으로 뻗는 이펙트는 대개 원점이 시작점에 있다. 몸 한가운데(0)에서 나오면 이상해
	 * 보이므로 캡슐 반경쯤(40~60)을 주면 칼끝·손 근처에서 시작한다.
	 *
	 * 앞쪽에서 **터지는** 이펙트라면 Range 의 절반을 주면 판정 상자 중심에 놓인다.
	 * 한 열로 두 경우를 다 덮기 위해 "상자 중심" 을 따로 두지 않았다.
	 *
	 * 맞출 때는 UTDSkillComponent 의 bDrawDebugShape 를 켜서 판정 상자와 겹쳐 본다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast")
	float VFXOffset = 0.f;

	/**
	 * ForwardBox 에서만 쓴다. 이펙트가 **몇 초에 걸쳐 사거리 끝까지 날아가는가**. 0 이면 제자리에 둔다.
	 *
	 * 앞으로 뻗는 이펙트는 두 종류다. 검기처럼 입자가 스스로 앞으로 날아가는 것은 제자리에
	 * 두면 되고(0), 화살처럼 **이펙트 자체가 움직여야 꼬리(리본)가 그려지는 것**은 코드가
	 * 옮겨 줘야 한다. 후자를 제자리에 두면 몸 앞에서 뿜기만 하고 나아가지 않는다.
	 *
	 * VFXOffset 에서 출발해 Range 에 닿으면 새 입자를 멈추고, 남은 입자는 스스로 사라진다.
	 *
	 * **표현만 날아간다.** 판정은 이 값과 무관하게 발동 순간에 난다 — 끝에 선 적도
	 * 이펙트가 닿기 전에 이미 맞는다. 짧게(0.2~0.3초) 두면 차이가 보이지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Cast", meta = (ClampMin = "0"))
	float VFXTravelTime = 0.f;
};
