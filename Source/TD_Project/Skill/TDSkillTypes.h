#pragma once

#include "CoreMinimal.h"
#include "TDSkillTypes.generated.h"

/**
 * 스킬의 종류.
 *
 * ── 태그가 아니라 enum 인 이유 ──
 * 식별자는 전부 FGameplayTag 로 둔다는 규칙(D9)의 예외다. 채팅 채널(D82)과 같은 판단이다.
 *
 * 액티브와 패시브는 **처리하는 코드가 전혀 겹치지 않는다.** 액티브는 시전·쿨다운·마나·판정을
 * 거치고, 패시브는 스탯 모디파이어를 등록하는 것이 전부다. 그리고 이 목록은 데이터가 늘어도
 * 늘지 않는다 — 스킬을 100 개 만들어도 종류는 둘이다.
 *
 * enum 이면 switch 에서 case 를 빠뜨렸을 때 컴파일러가 잡아준다.
 */
UENUM(BlueprintType)
enum class ETDSkillType : uint8
{
	/** 눌러서 쓴다. Q·W·E 에 배치되며 DT_SkillEffect 가 무슨 일이 일어날지 정한다. */
	Active		UMETA(DisplayName = "액티브"),

	/** 찍어 두면 항상 적용된다. DT_SkillPassive 의 모디파이어가 곧 효과다. */
	Passive		UMETA(DisplayName = "패시브")
};

/**
 * 액티브 스킬이 **어떻게 나가는가**. 패시브는 이 값을 쓰지 않는다.
 *
 * ── 왜 CastDelay 하나로 안 되는가 ──
 * 즉발과 캐스팅은 사실 같다 — 지연이 0 이냐 아니냐의 차이뿐이다. 하지만 셋을 가르면
 * **연출과 중단 규칙이 갈린다.** 즉발은 캐스팅 바가 아예 없고, 캐스팅은 차오르는 바,
 * 정신집중은 줄어드는 바다. UI 가 이걸 알아야 하므로 데이터에 있어야 한다.
 *
 * 정신집중은 그 위에 하나 더 다르다 — **판정이 여러 번 일어난다.** 반복 타이머와
 * 중간 취소 처리가 필요해서, 앞의 둘과 코드가 실제로 갈리는 유일한 지점이다.
 *
 * enum 인 이유는 ETDSkillType 과 같다(D82). 데이터가 늘어도 종류는 셋이고,
 * switch 에서 case 를 빠뜨리면 컴파일러가 잡는다.
 */
UENUM(BlueprintType)
enum class ETDSkillCastType : uint8
{
	/**
	 * 누르면 바로 나간다. 캐스팅 바가 없고 취소할 수 없다.
	 *
	 * CastDelay 는 **모션 안의 타격 지연**으로 쓰인다 — 평타의 HitDelay 와 같은 것으로,
	 * 칼을 휘두르는 순간에 판정을 맞추기 위한 값이지 기다리는 시간이 아니다.
	 */
	Instant		UMETA(DisplayName = "즉발"),

	/**
	 * CastDelay 만큼 기다린 뒤 한 번 나간다. 그 동안 캐스팅 바가 차오르고 취소할 수 있다.
	 */
	Cast		UMETA(DisplayName = "캐스팅"),

	/**
	 * CastDelay 뒤부터 ChannelDuration 동안 ChannelInterval 마다 반복해서 나간다.
	 *
	 * 마나는 **시작할 때 한 번** 소모한다. 틱마다 깎으면 도중에 마나가 떨어졌을 때를
	 * 처리해야 하고, 서버가 매 틱 검증해야 한다 — 지금 필요한 복잡도가 아니다.
	 */
	Channel		UMETA(DisplayName = "정신집중")
};

/**
 * 시전하는 동안 움직일 수 있는가. 스킬마다 다르므로 데이터로 둔다.
 *
 * 전역 규칙으로 두면 "회전베기는 돌면서 이동" 과 "레이저빔은 제자리" 를 동시에
 * 만족시킬 수 없다.
 *
 * bool 두 개(이동 가능 / 회전 가능)가 아니라 enum 인 이유는 "이동은 되는데 회전은 안 됨"
 * 이라는 조합이 이 게임에 없기 때문이다. 두 칸을 두면 시트에서 그 조합이 실수로 만들어진다.
 */
UENUM(BlueprintType)
enum class ETDSkillCastMovement : uint8
{
	/** 이동·회전 모두 자유. 시전 중에도 평소처럼 뛰어다닌다. 회전베기가 이쪽이다. */
	Free		UMETA(DisplayName = "자유"),

	/**
	 * 제자리에 서지만 방향은 바꿀 수 있다. 레이저처럼 겨냥을 따라가는 스킬용.
	 *
	 * 캐릭터가 이동 방향으로 회전하는 구조(bOrientRotationToMovement)라서, 이동만 막으면
	 * 회전도 함께 멈춘다. 이 값일 때는 이동 입력을 회전으로만 돌려야 한다.
	 */
	TurnOnly	UMETA(DisplayName = "방향 전환만"),

	/** 이동·회전 모두 막힌다. 시전 시작 시점의 방향으로 고정된다. */
	Locked		UMETA(DisplayName = "고정")
};

/**
 * 효과 하나가 **누구에게** 가는가.
 *
 * ── 스킬이 아니라 효과의 속성인 이유 ──
 * 한 스킬이 아군과 적을 동시에 다룰 수 있다. 마법사의 장판이 그렇다 —
 * 같은 시전으로 팀원은 회복하고 적은 피해를 입는다. 게다가 **회복량과 피해 배율이
 * 서로 다른 값이어야 한다.**
 *
 *   RowName          SkillId     EffectTag           TargetTeam  BaseValue
 *   Mage_Field_Heal  Mage_Field  Skill.Effect.Heal   Ally        80
 *   Mage_Field_Dmg   Mage_Field  Skill.Effect.Damage Enemy       0.8
 *
 * DT_Skill 에 두면 스킬 하나에 값이 하나뿐이라 이걸 표현할 수 없다.
 *
 * enum 인 이유는 ETDSkillType 과 같다(D82). 대상을 고르는 코드가 완전히 갈리고,
 * 데이터가 늘어도 종류는 둘이다.
 */
UENUM(BlueprintType)
enum class ETDSkillTarget : uint8
{
	/** 적만. 팀이 다른 살아 있는 캐릭터를 모은다. 기본값이다. */
	Enemy		UMETA(DisplayName = "적"),

	/**
	 * 같은 팀만. **시전자 자신도 포함한다.**
	 *
	 * 빼면 "파티원 회복" 이 자기만 빼고 도는 이상한 스킬이 된다. 혼자 있을 때
	 * 아무 일도 일어나지 않는 것도 곤란하다.
	 */
	Ally		UMETA(DisplayName = "아군")
};
