# DT_OptionDefinition · DT_OptionPool 생성.
#
# 3개 풀 × 4개 등급이면 행이 160개가 넘는다. 손으로 쓰면 "전설 공격력을 1% 올리자" 는
# 조정 하나에 12행을 찾아 고쳐야 하고, 한 곳을 빠뜨리면 등급 순서가 뒤집힌다.
# DT_Enhance(GenEnhanceCurve.py)·DT_LevelExp(GenLevelExpCurve.py)와 같은 방식이다.
#
# 사용법:
#   python GenOptionTables.py
#   결과는 Content/Data/DataTables/Item/CSV/GoogleSheet/ 에 덮어쓴다.
#
# 조정할 것은 아래 "조정 구간" 뿐이다. 그 아래는 표를 찍어내는 코드다.

import csv
import io
import os


# ═══════════════════════════════════════════════════════════════
#  조정 구간
# ═══════════════════════════════════════════════════════════════

# 풀 = 아이템 착용 레벨제한 구간. 아이템이 DT_ItemDefinition.OptionPoolId 로 하나를 가리킨다.
#
#   Pool_Lv01   1~19
#   Pool_Lv20   20~39
#   Pool_Lv40   40~50
POOLS = ["Pool_Lv01", "Pool_Lv20", "Pool_Lv40"]

RARITIES = ["Common", "Rare", "Epic", "Legendary"]


# ── 퍼센트 옵션의 최댓값(%) ──
#
# 등급 사이가 주된 격차이고(2 → 11), 풀 사이는 한 칸씩만 오른다(2 → 3 → 4).
# 레벨이 높다고 옵션이 몇 배가 되면 저레벨 장비를 쓸 이유가 사라진다.
PCT_MAX = {
    "Pool_Lv01": {"Common": 2, "Rare": 5, "Epic": 8, "Legendary": 11},
    "Pool_Lv20": {"Common": 3, "Rare": 6, "Epic": 9, "Legendary": 12},
    "Pool_Lv40": {"Common": 4, "Rare": 7, "Epic": 10, "Legendary": 13},
}

# 최솟값 = 최댓값 - 이 값. 굴릴 때의 폭이다.
PCT_SPREAD = 2


# ── 고정값 옵션의 레벨 배율 ──
#
# 퍼센트와 달리 고정값은 레벨을 따라 크게 올라야 한다. 40레벨에 체력이 2000인데
# +60을 주는 옵션은 없는 것과 같다.
FLAT_POOL_MULTIPLIER = {"Pool_Lv01": 1.0, "Pool_Lv20": 6.0, "Pool_Lv40": 15.0}

# 등급 배율. Common 을 1로 두고 위로 올라간다.
FLAT_RARITY_MULTIPLIER = {"Common": 1.0, "Rare": 2.5, "Epic": 6.0, "Legendary": 10.0}


# ── 옵션 종류 ──
#
# FLAT  : (표시명, 스탯태그, Common·Pool_Lv01 기준 Min, Max)
#         실제 값 = 기준 × 풀 배율 × 등급 배율
FLAT_OPTIONS = {
    "Hp":       ("최대 체력 +{0}",        "Stat.Resource.Health.Max", 30, 60),
    "Mp":       ("최대 마나 +{0}",        "Stat.Resource.Mana.Max", 20, 40),
    "Armor":    ("방어력 +{0}",           "Stat.Defense.Armor", 5, 12),
    "Phys":     ("물리공격력 +{0}",       "Stat.Offense.Damage.Physical", 3, 8),
    "Magic":    ("마법공격력 +{0}",       "Stat.Offense.Damage.Magical", 3, 8),
    "HpRegen":  ("초당 체력 재생 +{0}",   "Stat.Resource.Health.Regen", 1, 3),
    "MpRegen":  ("초당 마나 재생 +{0}",   "Stat.Resource.Mana.Regen", 1, 2),
}

# PCT   : (표시명, 스탯태그, Op, 배수)
#         실제 값 = PCT_MAX 표 × 배수. 확률 스탯은 %와 같은 축이라 여기 둔다.
#
# Op 가 Added 인 것들(치명타 확률 등)은 스탯 자체가 0~1 비율이라 더하기가 맞다.
# Increased 는 곱셈 계열이므로 "공격력 +10%" 처럼 기존 값에 비례해 붙는다.
PCT_OPTIONS = {
    "HpPct":     ("최대 체력 +{0}%",       "Stat.Resource.Health.Max", "Increased", 1.0),
    "PhysPct":   ("물리공격력 +{0}%",      "Stat.Offense.Damage.Physical", "Increased", 1.0),
    "MagicPct":  ("마법공격력 +{0}%",      "Stat.Offense.Damage.Magical", "Increased", 1.0),
    "DmgPct":    ("공격력 +{0}%",          "Stat.Offense.Damage", "Increased", 1.0),
    "Move":      ("이동속도 +{0}%",        "Stat.Utility.MoveSpeed", "Increased", 0.4),
    "Crit":      ("치명타 확률 +{0}%",     "Stat.Offense.CritChance", "Added", 0.7),
    "ArmorPen":  ("방어력 무시 +{0}%",     "Stat.Offense.ArmorPenetration", "Added", 0.7),
    "Reduce":    ("받는 피해 감소 +{0}%",  "Stat.Defense.DamageReduction", "Added", 0.5),
    "BossDmg":   ("보스 피해 +{0}%",       "Stat.Offense.BossDamage", "Added", 1.5),
    "Cooldown":  ("쿨다운 회복 +{0}%",     "Stat.Utility.CooldownRecoveryRate", "Added", 0.8),
    "CritDmg":   ("치명타 피해 +{0}",      "Stat.Offense.CritDamage", "Added", 2.0),
}


# ── 어느 (풀, 등급) 에 어떤 옵션이 들어가는가 ──
#
# **여기가 등급 게이팅이다.** 목록에 없으면 그 조합에서는 절대 나오지 않는다.
# 쿨다운·공격력%처럼 후반에 열려야 하는 것은 Pool_Lv40 의 Legendary 에만 있다.
#
# 저레벨 전설을 "수치만 낮은 전설" 이 아니라 "종류가 적은 전설" 로 만드는 장치다.
LAYOUT = {
    "Pool_Lv01": {
        "Common":    ["Hp", "Mp", "Armor", "Phys", "Magic"],
        "Rare":      ["Hp", "Armor", "HpRegen", "HpPct", "PhysPct", "MagicPct", "Move"],
        "Epic":      ["HpPct", "PhysPct", "MagicPct", "Crit", "ArmorPen", "MpRegen"],
        "Legendary": ["HpPct", "PhysPct", "MagicPct", "Crit", "ArmorPen", "Reduce"],
    },
    "Pool_Lv20": {
        "Common":    ["Hp", "Mp", "Armor", "Phys", "Magic"],
        "Rare":      ["Hp", "Armor", "HpRegen", "HpPct", "PhysPct", "MagicPct", "Move"],
        "Epic":      ["HpPct", "PhysPct", "MagicPct", "Crit", "ArmorPen", "MpRegen", "BossDmg"],
        "Legendary": ["HpPct", "PhysPct", "MagicPct", "Crit", "ArmorPen", "Reduce",
                      "CritDmg", "BossDmg"],
    },
    "Pool_Lv40": {
        "Common":    ["Hp", "Mp", "Armor", "Phys", "Magic"],
        "Rare":      ["Hp", "Armor", "HpRegen", "HpPct", "PhysPct", "MagicPct", "Move"],
        "Epic":      ["HpPct", "PhysPct", "MagicPct", "Crit", "ArmorPen", "MpRegen", "BossDmg"],
        "Legendary": ["HpPct", "PhysPct", "MagicPct", "Crit", "ArmorPen", "Reduce",
                      "CritDmg", "BossDmg", "Cooldown", "DmgPct"],
    },
}


# ── 뽑힐 가중치 ──
#
# 강한 옵션일수록 낮게 둔다. 같은 등급 안에서도 "잘 뽑힌 전설" 과 "그냥 전설" 이 갈린다.
WEIGHTS = {
    "Hp": 100, "Mp": 100, "Armor": 80, "Phys": 60, "Magic": 60,
    "HpRegen": 50, "MpRegen": 50,
    "HpPct": 50, "PhysPct": 40, "MagicPct": 40, "Move": 25,
    "Crit": 30, "ArmorPen": 30, "Reduce": 25,
    "BossDmg": 20, "CritDmg": 20, "Cooldown": 20, "DmgPct": 10,
}


# ═══════════════════════════════════════════════════════════════
#  생성
# ═══════════════════════════════════════════════════════════════

OUTPUT_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "Content", "Data", "DataTables", "Item", "CSV", "GoogleSheet")


def option_id(pool, rarity, key):
    """Opt_Lv20_HpPct_Epic — 풀·스탯·등급이 이름에 다 들어간다."""
    return f"Opt_{pool.replace('Pool_', '')}_{key}_{rarity}"


def trim(value):
    """4.0 을 '4' 로, 0.035 를 '0.035' 로. 시트에서 읽기 좋게 군더더기를 없앤다."""
    text = f"{value:.4f}".rstrip("0").rstrip(".")
    return text if text else "0"


def build_definitions():
    rows = []

    for pool in POOLS:
        for rarity in RARITIES:
            for key in LAYOUT[pool][rarity]:
                name = option_id(pool, rarity, key)

                if key in FLAT_OPTIONS:
                    display, tag, base_min, base_max = FLAT_OPTIONS[key]
                    scale = FLAT_POOL_MULTIPLIER[pool] * FLAT_RARITY_MULTIPLIER[rarity]
                    rows.append([name, display, tag, "Added",
                                 trim(round(base_min * scale)), trim(round(base_max * scale))])
                    continue

                display, tag, op, factor = PCT_OPTIONS[key]

                # 퍼센트 표는 사람이 읽는 % 단위(12)지만 스탯은 비율(0.12)로 쓴다.
                high = PCT_MAX[pool][rarity] * factor / 100.0
                low = max(0.0, (PCT_MAX[pool][rarity] - PCT_SPREAD) * factor / 100.0)

                rows.append([name, display, tag, op, trim(low), trim(high)])

    return rows


def build_pool():
    rows = []

    for pool in POOLS:
        for rarity in RARITIES:
            for key in LAYOUT[pool][rarity]:
                rows.append([
                    f"{pool.replace('Pool_', '')}_{rarity}_{key}",
                    pool,
                    f"Item.Rarity.{rarity}",
                    option_id(pool, rarity, key),
                    WEIGHTS[key],
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
    return path


def main():
    print("옵션 테이블을 생성한다...")

    write_csv("DT_OptionDefinition.csv",
              ["Name", "DisplayName", "StatTag", "Op", "MinValue", "MaxValue"],
              build_definitions())

    write_csv("DT_OptionPool.csv",
              ["Name", "PoolId", "Rarity", "OptionId", "Weight"],
              build_pool())

    print("완료. 에디터에서 두 테이블을 다시 임포트할 것.")


main()
