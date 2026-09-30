# DT_ItemDefinition(장신구 부분) · DT_ItemStat · DT_ItemSet · DT_ItemSetBonus ·
# DT_ItemUseEffect · DT_DropTable 생성.
#
# 세트 5개에 아이템이 39종, 아이템마다 스탯이 2행씩 붙어 행이 120개를 넘는다.
# 손으로 쓰면 "중급 물리공격력을 30에서 32로" 라는 조정 하나에 12행을 찾아 고쳐야 하고,
# 한 곳을 빠뜨리면 부위 사이 서열이 조용히 뒤집힌다.
# GenOptionTables.py · GenSkillTables.py 와 같은 방식이다.
#
# ── DT_ItemDefinition 은 왜 통째로 만들지 않는가 ──
# 소비 아이템과 퀘스트 아이템이 같은 테이블에 있다. 전부 찍어내면 남의 행까지
# 이 스크립트가 소유하게 되고, 포션 설명 한 줄을 고치려고 생성기를 열게 된다.
# 그래서 내보낸 CSV 를 읽어 **장신구 행만 갈아끼우고** 나머지는 그대로 흘려보낸다.
#
# Description 과 Icon 도 기존 값을 가져온다. 사람이 쓴 문장이라 여기서 지어내지 않는다.
# 새로 생기는 아이템은 둘 다 비어 나오므로 에디터에서 채우면 되고, 한 번 채운 뒤
# 다시 Export 해두면 그다음 생성부터 보존된다.
#
# 사용법:
#   1. ExportTables.bat          ← 현재 테이블을 CSV 로 내린다(설명·아이콘을 읽기 위해서다)
#   2. python GenItemTables.py
#   3. 에디터에서 여섯 테이블을 다시 임포트
#
# 조정할 것은 아래 "조정 구간" 뿐이다.

import csv
import io
import os


# ═══════════════════════════════════════════════════════════════
#  조정 구간
# ═══════════════════════════════════════════════════════════════

# ── 티어 ──
#
# 티어 하나가 곧 착용 레벨 구간이고, 그 구간의 옵션 풀과 짝이다.
# 기준값은 "이 티어를 쓰는 구간 한가운데에서 6칸을 채웠을 때 주스탯의 40~50%" 에서 나왔다.
# 예) 중급(20~39레벨)의 한가운데는 30레벨, 그때 전사 물리공격력이 362 다.
#
#   (RequiredLevel, OptionPoolId, Rarity, 표시 접두, 주스탯 기준값)
TIERS = {
    "Low":    (2,  "Pool_Lv01", "Common", "하급", 10),
    "Mid":    (20, "Pool_Lv20", "Rare",   "중급", 30),
    "High":   (40, "Pool_Lv40", "Epic",   "상급", 45),
    "Legend": (50, "Pool_Lv40", "Legendary", "전설", 55),   # 보스 세트 전용
}

# 되팔기 기준값. 티어별로 균일하다 — 부위마다 값을 다르게 하면 규칙을 설명할 수 없다.
SELL_PRICE = {"Low": 600, "Mid": 4500, "High": 18000, "Legend": 60000}


# ── 부위 ──
#
# 장착 칸에는 부위 구분이 없다(Item.Slot.* 을 만들지 않기로 했다). 그래서 부위는
# "같은 티어 안에서 무엇을 더 주느냐" 로만 구별된다. 셋을 모두 껴도 총량은 같다 —
# 주스탯 배율 합 3.0, 부스탯 배율 합 3.0.
#
#   (표시 이름, 주스탯 배율, 부스탯 배율)
SLOTS = {
    "Ring":    ("반지",   1.2, 0.5),
    "Pandant": ("팬던트", 0.8, 1.5),
    "Earring": ("귀걸이", 1.0, 1.0),
}


# ── 스탯 규모 ──
#
# 스탯마다 캐릭터가 가진 값의 크기가 다르다. 50레벨 전사는 물리공격력 602 에
# 방어력 1000, 체력 1850 이다. 물리공격력을 1.0 으로 두고 비례시킨다.
STAT_SCALE = {
    "Stat.Offense.Damage.Physical": 1.0,
    "Stat.Offense.Damage.Magical":  1.0,
    "Stat.Defense.Armor":           1.7,
    "Stat.Resource.Health.Max":     3.0,
    "Stat.Resource.Mana.Max":       3.0,
}

# ── 티어 기준값에 비례하지 않는 스탯 ──
#
# 확률은 레벨을 따라 커지지 않는다. 캐릭터 기본 치명타 확률은 50레벨에도 30% 그대로다.
# 공격력처럼 비례시키면 상급이 하급의 4.5배가 되는데, 정작 그것이 얹히는 바탕은
# 그대로여서 하급 쪽이 있으나 마나 해진다. 그래서 티어마다 직접 적는다.
#
# 부위 배율은 여기에도 걸린다 — 반지 0.5 / 팬던트 1.5 / 귀걸이 1.0.
ABSOLUTE_BY_TIER = {
    "Stat.Offense.CritChance": {"Low": 0.010, "Mid": 0.016, "High": 0.020, "Legend": 0.024},
}

# 소수로 적어야 하는 스탯. 나머지는 정수로 반올림한다.
FRACTIONAL_STATS = {"Stat.Offense.CritChance"}


# ── 세트 ──
#
# 세트 보너스는 **누적**이다(D32). 각 단계에는 그 단계에서 추가로 얻는 것만 적는다.
# 값은 전부 비율이다 — Added 고정값은 40레벨에 이르면 없는 것과 같아진다.
#
# 단, ArmorPenetration · CritChance · DamageReduction · BossDamage · CooldownRecoveryRate 는
# 그 자체가 이미 비율인 스탯이라 Added 로 더한다. Increased 로 넣으면
# "0 의 10% 증가" 가 되어 아무 일도 일어나지 않는다.
SETS = {
    "Fire_Set": {
        "display": "화염 세트",
        "prefix":  "Fire",
        "label":   "불의",
        "main":    "Stat.Offense.Damage.Physical",
        "sub":     "Stat.Resource.Health.Max",
        "bonus": [
            (2, "Stat.Offense.Damage.Physical", "Increased", 0.06, "Damage"),
            (4, "Stat.Offense.Damage.Physical", "Increased", 0.10, "Damage"),
            (6, "Stat.Offense.ArmorPenetration", "Added",    0.10, "ArmorPen"),
        ],
    },
    "Water_Set": {
        "display": "물 세트",
        "prefix":  "Water",
        "label":   "물의",
        "main":    "Stat.Offense.Damage.Magical",
        "sub":     "Stat.Resource.Mana.Max",
        "bonus": [
            (2, "Stat.Offense.Damage.Magical", "Increased", 0.06, "Damage"),
            (4, "Stat.Offense.Damage.Magical", "Increased", 0.10, "Damage"),
            (6, "Stat.Utility.CooldownRecoveryRate", "Added", 0.10, "Cooldown"),
        ],
    },
    "Earth_Set": {
        "display": "땅 세트",
        "prefix":  "Earth",
        "label":   "대지의",
        "main":    "Stat.Defense.Armor",
        "sub":     "Stat.Resource.Health.Max",
        "bonus": [
            (2, "Stat.Resource.Health.Max",      "Increased", 0.06, "Health"),
            (4, "Stat.Defense.Armor",            "Increased", 0.12, "Armor"),
            (6, "Stat.Defense.DamageReduction",  "Added",     0.06, "Reduce"),
        ],
    },
    "Wind_Set": {
        "display": "바람 세트",
        "prefix":  "Wind",
        "label":   "바람의",
        "main":    "Stat.Offense.Damage.Physical",
        "sub":     "Stat.Offense.CritChance",
        "bonus": [
            (2, "Stat.Offense.Damage.Physical", "Increased", 0.06, "Damage"),
            (4, "Stat.Offense.CritChance",      "Added",     0.08, "Crit"),
            (6, "Stat.Offense.CritDamage",      "Added",     0.25, "CritDmg"),
        ],
    },
}

# 일반 세트가 갖는 부위 × 티어. 보스 세트는 이 표를 따르지 않는다.
SET_TIERS = ["Low", "Mid", "High"]


# ── 보스 세트 ──
#
# 3종뿐이라 단계가 2/3 이다. 드롭처가 흩어져 있어(보스1 에서 둘, 보스2 에서 하나)
# 셋을 모으는 것 자체가 목표가 된다.
#
# 직업을 가리지 않도록 주스탯을 전체 공격력 비율로 둔다 — 물리·마법 어느 쪽이든
# 같은 값만큼 오른다. 부스탯은 체력이라 누가 껴도 손해가 없다.
#
# 등급은 티어를 따르지 않고 셋 다 전설이다. 드롭처가 보스라는 것 자체가 아이템의 격이고,
# 20제라고 해서 희귀로 내려가면 "보스가 준 물건" 이라는 표시가 사라진다.
#
# 표시 이름에도 티어 접두(하급/중급/상급)를 붙이지 않는다. 부위마다 레벨이 하나씩뿐이라
# 구별할 필요가 없고, "칠흑의 반지" 쪽이 이름으로 읽힌다.
#
#   아이템Id: (부위, 티어, 전체 공격력 증가율)
BOSS_SET_ID = "Boss_Set"
BOSS_SET_DISPLAY = "칠흑 세트"
BOSS_LABEL = "칠흑의"
BOSS_RARITY = "Legendary"

BOSS_ITEMS = {
    "DarkRing":    ("Ring",    "Mid",    0.04),
    "DarkPandant": ("Pandant", "High",   0.05),
    "DarkEarring": ("Earring", "Legend", 0.06),
}

# 전설 등급이라 같은 레벨대 일반 세트보다 세다.
BOSS_SUB_MULTIPLIER = 1.5

BOSS_BONUS = [
    (2, "Stat.Offense.Damage",      "Increased", 0.10, "Damage"),
    (3, "Stat.Offense.BossDamage",  "Added",     0.15, "BossDmg"),
]

# 보스 장신구는 처음부터 희귀 옵션이 붙어 나온다. 나머지는 가장 낮은 등급에서 시작한다.
BOSS_INITIAL_OPTION_RARITY = "Item.Rarity.Rare"
DEFAULT_INITIAL_OPTION_RARITY = "Item.Rarity.Common"


# ── 소비 아이템 효과 ──
#
# 포션 회복량의 바탕은 가격이다. 되팔기 기준가가 하급 50 : 중급 150 : 상급 300 이라
# 회복량이 그보다 완만하면 "싼 것을 여러 번 사는 쪽" 이 항상 이겨서 상급 포션이
# 상점에 놓여 있을 이유가 없어진다.
#
# 절대값의 기준은 그 포션을 쓰는 구간 한가운데의 최대 체력이다.
#   하급(1~14레벨)   7레벨 전사 345   →  150 이면 43%
#   중급(15~24)     20레벨 전사 800   →  500 이면 63%
#   상급(25~50)     37레벨 전사 1395  → 1000 이면 72%, 50레벨에도 54%
#
# 마나는 가격 비율(1 : 3 : 6)을 그대로 따른다. 마법사 마나가 전사 체력보다 조금 크고
# (50레벨 2070 vs 1850) 가격도 10% 비싸서 그만큼만 더 준다.
#
# 경험치 쪽은 아이템 설명에 숫자가 적혀 있다. 여기를 고치면 설명도 함께 고칠 것.
# 레벨업권의 Value 는 경험치가 아니라 **상한 레벨**이다 — 그 미만이면 한 칸 올려주고,
# 이상이면 그 레벨 한 구간만큼만 준다(UTDItemUseComponent::ApplyUseEffect).
#
#   (행 이름, 아이템Id, 효과 태그, 값)
USE_EFFECTS = [
    ("Healing_Low",   "HPotion_Low",     "Item.Effect.RestoreHealth",   150),
    ("Healing_Mid",   "HPotion_Mid",     "Item.Effect.RestoreHealth",   500),
    ("Healing_High",  "HPotion_High",    "Item.Effect.RestoreHealth",  1000),

    ("Mana_Low",      "MPotion_Low",     "Item.Effect.RestoreMana",     160),
    ("Mana_Mid",      "MPotion_Mid",     "Item.Effect.RestoreMana",     480),
    ("Mana_High",     "MPotion_High",    "Item.Effect.RestoreMana",     960),

    ("Exp_Low",       "ExpPotion_Low",   "Item.Effect.GainExp",         500),
    ("Exp_Mid",       "ExpPotion_Mid",   "Item.Effect.GainExp",        5000),

    ("Ticket_Low",    "LevelTicket_Low", "Item.Effect.LevelUp",          30),
    ("Ticket_Mid",    "LevelTicket_Mid", "Item.Effect.LevelUp",          40),

    ("Inv_Expand",    "InvExpand",       "Item.Effect.ExpandInventory",   5),
]


# ── 드롭 ──
#
# 목록은 몬스터별이 아니라 레벨 구간별이다. 같은 사냥터의 몬스터들이 하나를 나눠 쓰므로
# 몬스터가 늘어도 목록은 그대로고, "이 구간에서 뭐가 나오나" 를 한자리에서 본다.
# 어느 몬스터가 어느 목록을 쓰는지는 DT_MonsterDefinition.DropTableId 가 정한다.
#
# **확률은 잠정치다.** 제대로 맞추려면 "한 구간에서 몇 마리를 잡는가" 를 알아야 하고
# 그것은 몬스터 경험치에 달렸는데, DT_MonsterDefinition 이 아직 템플릿 상태다.
# 지금은 한 구간에 300마리쯤 잡는다고 보고 잡았다 — 장신구 한 종당 2% 면
# 300마리에 자기 직업 세트 3종이 여섯 번쯤 나온다.
#
#   티어: (목록 이름, 함께 나오는 소비 아이템 [(ItemId, 확률, Min, Max)], 장신구 한 종당 확률)
DROP_FIELD = {
    "Low":  ("Drop_Field_Lv01", [("HPotion_Low",  0.15, 1, 2), ("MPotion_Low",  0.10, 1, 2)], 0.02),
    "Mid":  ("Drop_Field_Lv20", [("HPotion_Mid",  0.15, 1, 2), ("MPotion_Mid",  0.10, 1, 2)], 0.02),
    "High": ("Drop_Field_Lv40", [("HPotion_High", 0.15, 1, 2), ("MPotion_High", 0.10, 1, 2)], 0.02),
}

# 보스 목록. 보스는 자주 잡을 수 없으니 확률이 높아야 한다 — 20번을 잡아야 반지 하나가
# 나온다면 세 종을 모으는 것이 사실상 불가능해진다.
#
# 칠흑 세트는 보스1 에서 둘, 보스2 에서 하나가 나온다.
DROP_BOSS = {
    "Drop_Boss_01": [("DarkRing", 0.50), ("DarkPandant", 0.30)],
    "Drop_Boss_02": [("DarkEarring", 0.30)],
}


# ── 이름이 바뀐 행 ──
#
# 반지만 _Normal 이었고 팬던트·귀걸이는 _Mid 였다. 접미사를 _Mid 로 통일하면서
# 옛 이름에 달려 있던 설명과 아이콘이 새 이름을 따라가게 한다.
RENAMED = {
    "FireRing_Normal": "FireRing_Mid",
}


# ═══════════════════════════════════════════════════════════════
#  여기부터는 표를 찍어내는 코드
# ═══════════════════════════════════════════════════════════════

OUTPUT_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "Content", "Data", "DataTables", "Item", "CSV", "GoogleSheet")

ITEM_DEFINITION_HEADER = [
    "Name", "DisplayName", "Description", "ItemType", "Icon",
    "bStackable", "MaxStackSize", "bCanDiscard", "SellPrice", "RequiredLevel",
    "Rarity", "InitialOptionRarity", "OptionPoolId", "SetId",
]

ACCESSORY_TYPE = "Item.Type.Accessory"


def trim(value):
    """4.0 을 '4' 로, 0.035 를 '0.035' 로. 시트에서 읽기 좋게 군더더기를 없앤다."""
    text = f"{value:.4f}".rstrip("0").rstrip(".")
    return text if text else "0"


def stat_value(tag, tier, slot_multiplier, extra=1.0):
    """티어 기준값 × 스탯 규모 × 부위 배율. 확률 스탯만 소수를 남긴다."""
    absolute = ABSOLUTE_BY_TIER.get(tag)

    if absolute is not None:
        raw = absolute[tier] * slot_multiplier * extra
    else:
        raw = TIERS[tier][4] * STAT_SCALE[tag] * slot_multiplier * extra

    if tag in FRACTIONAL_STATS:
        return trim(round(raw, 4))

    return str(int(round(raw)))


def accessory_rows():
    """
    (아이템Id, 표시이름, 티어, 주스탯, 주값, 부스탯, 부값, 세트Id, 옵션등급) 목록.

    일반 세트와 보스 세트를 한 줄기로 합쳐 아래 세 표가 모두 이것만 보게 한다.
    """
    items = []

    for set_id, spec in SETS.items():
        for tier in SET_TIERS:
            tier_label = TIERS[tier][3]

            for slot, (slot_label, main_mul, sub_mul) in SLOTS.items():
                items.append((
                    f"{spec['prefix']}{slot}_{tier}",
                    f"{tier_label} {spec['label']}{slot_label}",
                    tier,
                    spec["main"], stat_value(spec["main"], tier, main_mul), "Added",
                    spec["sub"],  stat_value(spec["sub"],  tier, sub_mul),  "Added",
                    set_id,
                    DEFAULT_INITIAL_OPTION_RARITY,
                ))

    # 보스 세트는 부위 배율을 쓰지 않는다. 셋을 다 모으는 것이 목표라 부위별 우열이
    # 없어야 하고, 배율을 태우면 50제 귀걸이가 40제 팬던트보다 약해진다.
    for item_id, (slot, tier, damage_pct) in BOSS_ITEMS.items():
        slot_label = SLOTS[slot][0]

        items.append((
            item_id,
            f"{BOSS_LABEL} {slot_label}",
            tier,
            # 주스탯은 비율이라 규모 표를 타지 않는다.
            "Stat.Offense.Damage", trim(damage_pct), "Increased",
            "Stat.Resource.Health.Max",
            stat_value("Stat.Resource.Health.Max", tier, 1.0, BOSS_SUB_MULTIPLIER), "Added",
            BOSS_SET_ID,
            BOSS_INITIAL_OPTION_RARITY,
        ))

    return items


def read_existing_definitions():
    """
    지금 DT_ItemDefinition 에 있는 행. Name 을 키로 돌려준다.

    없으면 빈 것으로 본다 — 처음 돌리는 사람이 Export 부터 하도록 강요할 이유는 없고,
    그때는 설명과 아이콘이 비어 나올 뿐이다.
    """
    path = os.path.abspath(os.path.join(OUTPUT_DIR, "DT_ItemDefinition.csv"))
    if not os.path.exists(path):
        print("  (DT_ItemDefinition.csv 가 없다 — 설명·아이콘 없이 새로 만든다)")
        return {}, []

    with io.open(path, "r", encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.reader(handle))

    if not rows:
        return {}, []

    header = rows[0]
    index = {name: position for position, name in enumerate(header)}

    def cell(row, column):
        position = index.get(column)
        return row[position] if position is not None and position < len(row) else ""

    existing = {}
    others = []

    for row in rows[1:]:
        if not row or not row[0].strip():
            continue

        name = RENAMED.get(row[0], row[0])
        existing[name] = {column: cell(row, column) for column in header}

        # 장신구가 아닌 행은 손대지 않고 그대로 다시 쓴다.
        if cell(row, "ItemType") != ACCESSORY_TYPE:
            others.append([cell(row, column) for column in ITEM_DEFINITION_HEADER])

    return existing, others


def build_item_definition(items, existing, others):
    rows = list(others)

    for (item_id, display, tier, _, _, _, _, _, _, set_id, option_rarity) in items:
        required_level, pool_id, tier_rarity, _, _ = TIERS[tier]
        rarity = BOSS_RARITY if set_id == BOSS_SET_ID else tier_rarity
        previous = existing.get(item_id, {})

        rows.append([
            item_id,
            display,
            previous.get("Description", ""),
            ACCESSORY_TYPE,
            previous.get("Icon", "None"),
            "False",                      # 개체마다 강화·옵션이 달라 겹칠 수 없다
            "1",
            "True",
            str(SELL_PRICE[tier]),
            str(required_level),
            f"Item.Rarity.{rarity}",
            option_rarity,
            pool_id,
            set_id,
        ])

    return rows


def build_item_stat(items):
    rows = []

    for (item_id, _, _, main_tag, main_value, main_op,
         sub_tag, sub_value, sub_op, _, _) in items:
        rows.append([f"{item_id}_Main", item_id, main_tag, main_op, main_value])
        rows.append([f"{item_id}_Sub",  item_id, sub_tag,  sub_op,  sub_value])

    return rows


def build_item_set():
    rows = [[set_id, spec["display"]] for set_id, spec in SETS.items()]
    rows.append([BOSS_SET_ID, BOSS_SET_DISPLAY])
    return rows


def build_item_set_bonus():
    rows = []

    for set_id, spec in SETS.items():
        for count, tag, op, value, short in spec["bonus"]:
            rows.append([f"{spec['prefix']}_{count}_{short}", set_id, str(count), tag, op, trim(value)])

    for count, tag, op, value, short in BOSS_BONUS:
        rows.append([f"Boss_{count}_{short}", BOSS_SET_ID, str(count), tag, op, trim(value)])

    return rows


def build_drop_table():
    rows = []

    for tier, (table_id, consumables, accessory_chance) in DROP_FIELD.items():
        prefix = table_id.replace("Drop_", "")

        for item_id, chance, min_count, max_count in consumables:
            rows.append([f"{prefix}_{item_id}", table_id, item_id,
                         trim(chance), str(min_count), str(max_count)])

        # 장신구는 개체마다 옵션이 달라 겹치지 않는다. 개수는 반드시 1 이다.
        for spec in SETS.values():
            for slot in SLOTS:
                item_id = f"{spec['prefix']}{slot}_{tier}"
                rows.append([f"{prefix}_{item_id}", table_id, item_id,
                             trim(accessory_chance), "1", "1"])

    for table_id, drops in DROP_BOSS.items():
        prefix = table_id.replace("Drop_", "")

        for item_id, chance in drops:
            rows.append([f"{prefix}_{item_id}", table_id, item_id, trim(chance), "1", "1"])

    return rows


def build_item_use_effect():
    return [[name, item_id, tag, trim(value)] for name, item_id, tag, value in USE_EFFECTS]


def write_csv(filename, header, rows):
    path = os.path.abspath(os.path.join(OUTPUT_DIR, filename))

    # 언리얼 임포터는 BOM 이 있는 UTF-8 을 기대한다. 없으면 한글이 깨진다.
    with io.open(path, "w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.writer(handle, lineterminator="\r\n")
        writer.writerow(header)
        writer.writerows(rows)

    print(f"  {filename:<28} {len(rows)}행")


def main():
    print("아이템 테이블을 생성한다...")

    existing, others = read_existing_definitions()
    items = accessory_rows()

    write_csv("DT_ItemDefinition.csv", ITEM_DEFINITION_HEADER,
              build_item_definition(items, existing, others))

    write_csv("DT_ItemStat.csv", ["Name", "ItemId", "StatTag", "Op", "Value"],
              build_item_stat(items))

    write_csv("DT_ItemSet.csv", ["Name", "DisplayName"], build_item_set())

    write_csv("DT_ItemSetBonus.csv",
              ["Name", "SetId", "RequiredCount", "StatTag", "Op", "Value"],
              build_item_set_bonus())

    write_csv("DT_ItemUseEffect.csv", ["Name", "ItemId", "EffectTag", "Value"],
              build_item_use_effect())

    write_csv("DT_DropTable.csv",
              ["Name", "DropTableId", "ItemId", "Chance", "MinCount", "MaxCount"],
              build_drop_table())

    print(f"\n장신구 {len(items)}종 (일반 {len(SETS) * len(SET_TIERS) * len(SLOTS)}종 + "
          f"보스 {len(BOSS_ITEMS)}종), 소비·퀘스트 {len(others)}행 보존.")
    print("에디터에서 여섯 테이블을 다시 임포트할 것.")


main()
