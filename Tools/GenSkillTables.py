# DT_Skill · DT_SkillEffect · DT_SkillPassive 를 함께 생성한다.
#
# ── 왜 세 장을 한 스크립트가 만드는가 ──
# 셋이 SkillId 로 물려 있다. 스킬 이름을 바꾸면 세 파일을 같이 고쳐야 하고,
# 한 곳을 빠뜨리면 "시전은 되는데 아무 일도 안 일어나는" 스킬이 조용히 생긴다.
#
# ── 값을 어떻게 적는가 ──
# 코드는 `Base + PerLevel × (레벨 - 1)` 로 계산하지만, 사람이 읽을 때 궁금한 것은
# **1레벨과 만렙에 얼마인가**다. 그래서 표에는 (1레벨, 만렙) 을 적고 PerLevel 은
# 여기서 역산한다. "전사 강타를 만렙 350% 로" 가 숫자 하나를 고치는 일이 된다.
#
# 사용법:
#   python GenSkillTables.py
#   결과는 Content/Data/DataTables/Skill/CSV/GoogleSheet/ 에 덮어쓴다.

import csv
import io
import os


# ═══════════════════════════════════════════════════════════════
#  조정 구간
# ═══════════════════════════════════════════════════════════════

# 스킬 하나의 최대 레벨. 6종 × 25 = 150 이고, 캐릭터 만렙 50 × 3포인트 = 150 이라
# 만렙에 정확히 다 찍힌다. 가는 도중에는 무엇을 먼저 올릴지 고르게 된다.
MAX_SKILL_LEVEL = 25

# 액티브가 열리는 캐릭터 레벨. 패시브는 처음부터 찍을 수 있다.
UNLOCK_LEVEL = {1: 1, 2: 10, 3: 20}

# 슬롯별 기본 쿨타임(초). 개별 스킬이 덮어쓸 수 있다.
#
#   1번  평딜기. 계속 돌린다
#   2번  16초 이내
#   3번  극딜기. 90초 이내로 두되 60초로 잡았다 —
#        90초면 초당 기여가 평딜기의 절반이라 실질적으로 안 쓰게 된다
SLOT_COOLDOWN = {1: 5.0, 2: 15.0, 3: 60.0}

# 슬롯별 만렙 피해 배율(공격력의 몇 배). 1레벨은 이 값의 40% 에서 시작한다.
SLOT_DAMAGE_AT_MAX = {1: 3.0, 2: 8.0, 3: 25.0}
DAMAGE_START_RATIO = 0.4


# ── 액티브 ────────────────────────────────────────────────────
#
# (SkillId, 표시명, 설명, 직업, 슬롯, 모양, 사거리, 폭, 시전타입, 시전지연,
#  정신집중 지속, 정신집중 간격, 이동제약, 마나(1레벨, 만렙), 쿨 덮어쓰기)
#
# 설명의 {이름} 은 UTDTooltipStatics 가 채운다(D102). 값 인자는 % 를 스스로 붙이므로
# 문장에 % 를 또 쓰면 "150%%" 가 된다.

ACTIVES = [
    # ── 전사 ──
    ("Warrior_Blade", "검기", "전방으로 검기를 날려 적을 관통하며 공격력의 {Damage} 피해를 준다.",
     "Warrior", 1, "ForwardBox", 300, 250, "Instant", 0.35, 0, 0, "Locked", (18, 60), None),

    # 방벽은 즉발이다. 캐스팅이면 맞는 도중에 끊겨 정작 필요할 때 무적이 안 걸린다.
    ("Warrior_Guard", "방벽", "방벽을 세워 {Duration}초 동안 피해를 받지 않고 체력을 {Heal} 회복한다.",
     "Warrior", 2, "Self", 0, 0, "Instant", 0.1, 0, 0, "Locked", (30, 90), None),

    # 폭풍베기는 몸 앞의 넓은 공간을 한 번에 터뜨린다. 이펙트는 상자 중심에 놓인다(VFX_OFFSET).
    ("Warrior_Cyclone", "폭풍베기", "앞의 공간을 터뜨려 공격력의 {Damage} 피해를 준다.",
     "Warrior", 3, "ForwardBox", 400, 300, "Instant", 0.5, 0, 0, "Locked", (60, 180), None),

    # ── 궁수 ──
    ("Archer_Pierce", "스나이핑", "적 하나를 꿰뚫어 공격력의 {Damage} 피해를 준다.",
     "Archer", 1, "ForwardBox", 700, 80, "Instant", 0.3, 0, 0, "Locked", (16, 55), None),

    ("Archer_Volley", "관통사격", "앞으로 길게 공격을 뻗어 공격력의 {Damage} 피해를 준다.",
     "Archer", 2, "ForwardBox", 700, 320, "Cast", 0.5, 0, 0, "Locked", (32, 95), None),

    ("Archer_Storm", "블리자드", "자신으로부터 뻗어나가는 얼음을 뽑아 공격력의 {Damage} 피해를 준다.",
     "Archer", 3, "SelfRadius", 600, 0, "Instant", 0.5, 0, 0, "Locked", (65, 190), None),

    # ── 마법사 ──
    ("Mage_Field", "치유의 늪", "주위에 장을 펼쳐 아군의 체력을 {Heal} 회복하고 적에게 공격력의 {Damage} 피해를 준다.",
     "Mage", 1, "SelfRadius", 400, 0, "Instant", 0.3, 0, 0, "Locked", (22, 70), None),

    ("Mage_Storm", "마력 폭풍", "{Duration}초 동안 주위에 폭풍을 일으켜 {Interval}초마다 공격력의 {Damage} 피해를 준다. "
                              "모두 {Ticks}번 적중한다.",
     "Mage", 2, "SelfRadius", 380, 0, "Channel", 0.4, 5.0, 0.5, "Free", (35, 100), None),

    ("Mage_Rally", "전열 강화", "{Duration}초 동안 주위 아군의 공격력을 {Physical} 올린다.",
     "Mage", 3, "SelfRadius", 500, 0, "Cast", 0.6, 0, 0, "Locked", (70, 200), None),
]

# 액티브의 추가 효과. 피해는 위 SLOT_DAMAGE_AT_MAX 로 자동 생성되므로 여기 적지 않는다.
#
# (SkillId, 접미사, 효과태그, 대상팀, (1레벨, 만렙), 지속시간, 스탯태그, 연산)

ACTIVE_EXTRA_EFFECTS = [
    # 방벽 — 무적은 시간이 고정이라 레벨을 올릴 이유가 없다. 회복이 그 몫을 한다.
    # 대신 쿨이 레벨을 따라 줄어든다(아래 COOLDOWN_SCALING).
    ("Warrior_Guard", "Invuln", "Skill.Effect.Invulnerable", "Ally", (0, 0), 1.0, "", "Added"),
    ("Warrior_Guard", "Heal", "Skill.Effect.Heal", "Ally", (60, 500), 0, "", "Added"),

    # 치유의 장 — 힐과 딜의 계수가 서로 다르다. 그래서 행을 나눈다.
    ("Mage_Field", "Heal", "Skill.Effect.Heal", "Ally", (50, 400), 0, "", "Added"),

    # 전열 강화 — Stat.Offense.Damage 는 상위 태그라 계층을 타지 않는다.
    # 물리·마법을 따로 올려야 세 직업 모두에게 걸린다.
    ("Mage_Rally", "Phys", "Skill.Effect.Buff", "Ally", (0.08, 0.30), 5.0,
     "Stat.Offense.Damage.Physical", "Increased"),
    ("Mage_Rally", "Magic", "Skill.Effect.Buff", "Ally", (0.08, 0.30), 5.0,
     "Stat.Offense.Damage.Magical", "Increased"),
]

# 피해 효과를 만들지 않을 스킬. 방벽과 전열 강화는 때리지 않는다.
NO_DAMAGE = {"Warrior_Guard", "Mage_Rally"}

# 레벨을 따라 쿨이 줄어드는 스킬. {SkillId: 만렙 쿨타임}
COOLDOWN_SCALING = {
    "Warrior_Guard": 12.0,   # 16 → 12. 무적 시간이 고정이라 이쪽으로 성장을 준다
}
# 방벽만 기본 쿨이 다르다(슬롯 기본 15 대신 16).
COOLDOWN_OVERRIDE = {
    "Warrior_Guard": 16.0,
}


# ── 이펙트 ────────────────────────────────────────────────────
#
# **에디터에서 VFX 를 넣었다면 여기에도 적을 것.** 이 스크립트로 CSV 를 다시 만들어
# 재임포트하면 행이 통째로 바뀌므로, 여기 없는 값은 비워진다.
#
#   VFX_PATHS        {SkillId: 에셋 경로}
#   VFX_BASE_SIZE    {SkillId: 이펙트가 원래 몇 cm 짜리인가}  Range / 이 값으로 키운다
#   VFX_OFFSET       {SkillId: 몸에서 몇 cm 앞에서 시작하는가}  ForwardBox 에서만
#   VFX_TRAVEL_TIME  {SkillId: 사거리 끝까지 몇 초에 날아가는가}  ForwardBox 에서만, 0 이면 제자리
#
# 값은 PIE 에서 TD.SkillVFX <스킬ID> <크기> [오프셋] [이동시간] 으로 맞춘 뒤 옮긴다.
# 판정 범위(bDrawDebugShape)와 겹쳐 보면서 숫자를 바꾸면 재임포트 없이 바로 보인다.

_SFX = "/Game/Art/SFX/NS_SFX_InUseSkill"

VFX_PATHS = {
    "Warrior_Blade":   f"{_SFX}/NS_Skill_Black/NS_Skill_Black1.NS_Skill_Black1",
    "Warrior_Guard":   f"{_SFX}/NS_Skill_Black/NS_Skill_Black2.NS_Skill_Black2",
    "Warrior_Cyclone": f"{_SFX}/NS_Skill_Black/NS_Skill_Black3.NS_Skill_Black3",
    "Archer_Pierce":   f"{_SFX}/NS_Skill_Pink/NS_Skill_Pink_1_1.NS_Skill_Pink_1_1",
    "Archer_Volley":   f"{_SFX}/NS_Skill_Pink/NS_Skill_Pink2.NS_Skill_Pink2",
    "Archer_Storm":    f"{_SFX}/NS_Skill_Pink/NS_Skill_Pink3.NS_Skill_Pink3",
    "Mage_Field":      f"{_SFX}/NS_Skill_Yellow/NS_Skill_Yellow1.NS_Skill_Yellow1",
    "Mage_Storm":      f"{_SFX}/NS_Skill_Yellow/NS_Skill_Yellow2.NS_Skill_Yellow2",
    "Mage_Rally":      f"{_SFX}/NS_Skill_Yellow/NS_Skill_Yellow3.NS_Skill_Yellow3",
}
VFX_BASE_SIZE = {}

VFX_OFFSET = {
    # 몸 앞 공간에서 터지는 이펙트라 판정 상자 중심(사거리 400 의 절반)에 둔다.
    # 사거리를 바꾸면 이 값도 절반으로 맞출 것.
    "Warrior_Cyclone": 200,

    # 화살은 손 근처에서 출발한다. 잠정치 — TD.SkillVFX 로 맞출 것.
    "Archer_Pierce": 50,
}

VFX_TRAVEL_TIME = {
    # 리본(꼬리)을 쓰는 에셋이라 이펙트 자체가 움직여야 앞으로 나간다.
    # 700cm 를 0.25초 — 잠정치다. TD.SkillVFX 로 맞출 것.
    "Archer_Pierce": 0.25,
}

# ── 효과음 ────────────────────────────────────────────────────
#
#   SFX_PATHS        {SkillId: 사운드 에셋 경로}  발동 순간(이펙트와 같은 순간)에 한 번 난다
#
# VFX 와 같은 규칙이다 — **에디터에서 넣었다면 여기에도 적을 것.** 재생성하면 비워진다.
# 경로는 "/Game/.../SW_Slash.SW_Slash" 형태. 사운드 에셋의 Submix 를 SM_SFX 로 둬야 볼륨 설정이 먹는다.
# (2026-09-15 기준 스킬 효과음 에셋이 아직 없어 비어 있다.)

SFX_PATHS = {}


# ── 패시브 ────────────────────────────────────────────────────
#
# (SkillId, 표시명, 직업, [(스탯태그, 연산, 1레벨, 만렙, 설명조각), ...])
#
# 설명은 조각을 이어 붙여 만든다. 인자 이름은 스탯 태그에서 파생되므로
# ("Stat.Defense.Armor" → {Armor}) 여기서 따로 적지 않는다.

PASSIVES = [
    # ── 전사 ── 버티는 직업
    ("Warrior_Endure", "인내", "Warrior", [
        ("Stat.Resource.Health.Regen", "Added", 10, 60, "초당 체력이 {Regen} 회복되고"),
        ("Stat.Defense.DamageReduction", "Added", 0.004, 0.10, "받는 피해가 {DamageReduction} 줄어든다."),
    ]),
    ("Warrior_Fortress", "요새", "Warrior", [
        ("Stat.Resource.Health.Max", "Increased", 0.005, 0.125, "최대 체력이 {Max} 오르고"),
        ("Stat.Defense.Armor", "Increased", 0.005, 0.125, "방어력이 {Armor} 오른다."),
    ]),
    ("Warrior_Might", "위력", "Warrior", [
        ("Stat.Offense.Damage.Physical", "Increased", 0.01, 0.25, "물리공격력이 {Physical} 오르고"),
        ("Stat.Offense.ArmorPenetration", "Added", 0.002, 0.05, "방어력 무시가 {ArmorPenetration} 오른다."),
    ]),

    # ── 궁수 ── 치명타 직업. 두 패시브의 치명타 확률을 합쳐 만렙 35%
    ("Archer_Focus", "집중", "Archer", [
        ("Stat.Offense.CritChance", "Added", 0.01, 0.25, "치명타 확률이 {CritChance} 오르고"),
        ("Stat.Offense.Damage.Physical", "Added", 4, 100, "물리공격력이 {Physical} 오른다."),
    ]),
    ("Archer_Hunter", "사냥꾼", "Archer", [
        ("Stat.Offense.BossDamage", "Added", 0.008, 0.20, "보스에게 주는 피해가 {BossDamage} 오르고"),
        ("Stat.Offense.CritChance", "Added", 0.004, 0.10, "치명타 확률이 {CritChance} 오른다."),
    ]),
    ("Archer_Precision", "정밀", "Archer", [
        ("Stat.Offense.Damage.Physical", "Increased", 0.01, 0.25, "물리공격력이 {Physical} 오르고"),
        ("Stat.Offense.CritDamage", "Added", 0.006, 0.15, "치명타 피해가 {CritDamage} 오른다."),
    ]),

    # ── 마법사 ── 마나와 배율의 직업
    ("Mage_Flow", "마나 순환", "Mage", [
        ("Stat.Resource.Mana.Regen", "Added", 10, 60, "초당 마나가 {Regen} 회복되고"),
        ("Stat.Utility.MoveSpeed", "Increased", 0.002, 0.05, "이동속도가 {MoveSpeed} 오른다."),
    ]),
    ("Mage_Amplify", "증폭", "Mage", [
        ("Stat.Offense.Damage.Magical", "Increased", 0.05, 0.20, "마법공격력이 {Magical} 오른다."),
    ]),
    ("Mage_Insight", "통찰", "Mage", [
        ("Stat.Offense.Damage.Magical", "Increased", 0.01, 0.25, "마법공격력이 {Magical} 오르고"),
        ("Stat.Utility.CooldownRecoveryRate", "Added", 0.0008, 0.02, "쿨다운 회복률이 {CooldownRecoveryRate} 오른다."),
    ]),
]


# ═══════════════════════════════════════════════════════════════
#  생성
# ═══════════════════════════════════════════════════════════════

OUTPUT_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "Content", "Data", "DataTables", "Skill", "CSV", "GoogleSheet")


def per_level(at_one, at_max):
    """만렙 목표값에서 레벨당 증가분을 역산한다. 레벨 1 은 증가가 없으므로 24 로 나눈다."""
    if MAX_SKILL_LEVEL <= 1:
        return 0.0
    return (at_max - at_one) / (MAX_SKILL_LEVEL - 1)


def trim(value):
    """4.0 을 '4' 로, 0.0625 를 '0.0625' 로. 시트에서 읽기 좋게 군더더기를 없앤다."""
    text = f"{value:.6f}".rstrip("0").rstrip(".")
    return text if text else "0"


def build_skill_rows():
    rows = []

    for (skill_id, name, description, class_id, slot, shape, rng, width,
         cast_type, cast_delay, duration, interval, movement, mana, cd_override) in ACTIVES:

        cooldown = cd_override or COOLDOWN_OVERRIDE.get(skill_id, SLOT_COOLDOWN[slot])
        cd_at_max = COOLDOWN_SCALING.get(skill_id)
        cd_per_level = per_level(cooldown, cd_at_max) if cd_at_max is not None else 0.0

        rows.append([
            skill_id, class_id, name, description, "",
            "Active", slot, MAX_SKILL_LEVEL, UNLOCK_LEVEL[slot],
            trim(mana[0]), trim(per_level(*mana)),
            trim(cooldown), trim(cd_per_level),
            f"Skill.Shape.{shape}", trim(rng), trim(width),
            cast_type, trim(cast_delay), trim(duration), trim(interval),
            movement, "()", VFX_PATHS.get(skill_id, ""),
            trim(VFX_BASE_SIZE.get(skill_id, 0)), trim(VFX_OFFSET.get(skill_id, 0)),
            trim(VFX_TRAVEL_TIME.get(skill_id, 0)),
            SFX_PATHS.get(skill_id, ""),
        ])

    for skill_id, name, class_id, stats in PASSIVES:
        description = " ".join(fragment for _, _, _, _, fragment in stats)

        rows.append([
            skill_id, class_id, name, description, "",
            "Passive", 0, MAX_SKILL_LEVEL, 1,
            "0", "0", "0", "0",
            "", "0", "0",
            "Instant", "0", "0", "0",
            "Free", "()", "", "0", "0", "0", "",
        ])

    return rows


def build_effect_rows():
    rows = []

    for (skill_id, _, _, _, slot, _, _, _, cast_type, _,
         channel_duration, channel_interval, _, _, _) in ACTIVES:

        if skill_id in NO_DAMAGE:
            continue

        at_max = SLOT_DAMAGE_AT_MAX[slot]
        at_one = at_max * DAMAGE_START_RATIO

        # 정신집중은 여러 번 때린다. 슬롯 배율은 "한 번의 시전으로 넣는 총량" 이므로
        # 타격 횟수로 나눠야 다른 스킬과 견줄 수 있다.
        if cast_type == "Channel" and channel_duration > 0 and channel_interval > 0:
            ticks = int(channel_duration / channel_interval + 1e-4)
            at_max /= ticks
            at_one /= ticks

        rows.append([
            f"{skill_id}_Dmg", skill_id, "Skill.Effect.Damage",
            trim(at_one), trim(per_level(at_one, at_max)),
            "Enemy", "0", "", "Added",
        ])

    for (skill_id, suffix, tag, team, values, duration, stat, op) in ACTIVE_EXTRA_EFFECTS:
        rows.append([
            f"{skill_id}_{suffix}", skill_id, tag,
            trim(values[0]), trim(per_level(*values)),
            team, trim(duration), stat, op,
        ])

    return rows


def build_passive_rows():
    rows = []

    for skill_id, _, _, stats in PASSIVES:
        for stat_tag, op, at_one, at_max, _ in stats:
            # 태그의 끝 조각을 행 이름에 쓴다. 한 패시브가 같은 스탯을 두 번 올리지 않으므로 겹치지 않는다.
            suffix = stat_tag.rsplit(".", 1)[-1]
            rows.append([
                f"{skill_id}_{suffix}", skill_id, stat_tag, op,
                trim(at_one), trim(per_level(at_one, at_max)),
            ])

    return rows


def write_csv(filename, header, rows):
    path = os.path.abspath(os.path.join(OUTPUT_DIR, filename))

    # 언리얼 임포터는 BOM 이 있는 UTF-8 을 기대한다. 없으면 한글이 깨진다.
    with io.open(path, "w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.writer(handle, lineterminator="\r\n")
        writer.writerow(header)
        writer.writerows(rows)

    print(f"  {filename}  {len(rows)}행")


def main():
    print(f"스킬 테이블을 생성한다 (최대 레벨 {MAX_SKILL_LEVEL})...")

    write_csv("DT_Skill.csv",
              ["Name", "ClassId", "DisplayName", "Description", "Icon",
               "SkillType", "SlotIndex", "MaxLevel", "RequiredLevel",
               "ManaCost", "ManaCostPerLevel", "Cooldown", "CooldownPerLevel",
               "ShapeTag", "Range", "Width",
               "CastType", "CastDelay", "ChannelDuration", "ChannelInterval",
               "CastMovement", "ContextTags", "VFX", "VFXBaseSize", "VFXOffset", "VFXTravelTime", "SFX"],
              build_skill_rows())

    write_csv("DT_SkillEffect.csv",
              ["Name", "SkillId", "EffectTag", "BaseValue", "ValuePerLevel",
               "TargetTeam", "Duration", "StatTag", "Op"],
              build_effect_rows())

    write_csv("DT_SkillPassive.csv",
              ["Name", "SkillId", "StatTag", "Op", "BaseValue", "ValuePerLevel"],
              build_passive_rows())

    print("완료. 에디터에서 세 테이블을 다시 임포트할 것.")

    # 만렙 도달값을 찍어 표와 맞는지 눈으로 확인한다.
    print("\n── 패시브 만렙 도달값 ──")
    for skill_id, name, class_id, stats in PASSIVES:
        parts = []
        for stat_tag, op, at_one, at_max, _ in stats:
            short = stat_tag.replace("Stat.", "")
            shown = f"{at_max * 100:g}%" if op in ("Increased", "More") or at_max <= 1 else f"{at_max:g}"
            parts.append(f"{short} {shown}")
        print(f"  {class_id:8} {name:10} " + "  ·  ".join(parts))


main()
