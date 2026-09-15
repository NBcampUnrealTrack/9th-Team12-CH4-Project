# DataTable 검증. 커밋·PR 전에 돌려 "조용히 틀리는" 데이터를 잡는다.
#
# ── 왜 CSV 가 아니라 .uasset 을 읽는가 ──
# 테이블 24개 중 CSV 가 있는 것은 절반뿐이고, DT_ItemDefinition 은 부분본만 있다.
# CSV 만 읽으면 가장 자주 깨지는 ItemId 참조를 검사할 수 없다. 게다가 누군가
# Export 를 빠뜨리면 낡은 데이터를 보고 "통과" 를 찍는데, 그게 검사가 없는 것보다 나쁘다.
#
# 그래서 에디터 안에서 돌며 진실의 원천을 그대로 읽는다.
#
# 사용법:
#   ValidateData.bat 을 더블클릭
#   또는  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="...\ValidateData.py"
#
# 결과: 문제가 있으면 목록을 찍고 마지막 줄에 실패를 남긴다.

import re

import unreal


ROOT = "/Game/Data/DataTables"

# 별칭 → 에셋 경로. 규칙에는 별칭만 쓴다.
TABLES = {
    "CharacterClass": f"{ROOT}/Character/DT_CharacterClass",
    "ClassGrowth":    f"{ROOT}/Character/DT_ClassGrowth",
    "UnionBonus":     f"{ROOT}/Character/DT_UnionBonus",
    "ItemDefinition": f"{ROOT}/Item/DT_ItemDefinition",
    "ItemStat":       f"{ROOT}/Item/DT_ItemStat",
    "ItemSet":        f"{ROOT}/Item/DT_ItemSet",
    "ItemSetBonus":   f"{ROOT}/Item/DT_ItemSetBonus",
    "ItemUseEffect":  f"{ROOT}/Item/DT_ItemUseEffect",
    "DropTable":      f"{ROOT}/Item/DT_DropTable",
    "Shop":           f"{ROOT}/Shop/DT_Shop",
    "ShopItem":       f"{ROOT}/Shop/DT_ShopItem",
    "Skill":          f"{ROOT}/Skill/DT_Skill",
    "SkillEffect":    f"{ROOT}/Skill/DT_SkillEffect",
    "SkillPassive":   f"{ROOT}/Skill/DT_SkillPassive",
    "OptionDefinition": f"{ROOT}/Item/DT_OptionDefinition",
    "OptionPool":       f"{ROOT}/Item/DT_OptionPool",

    # 아래는 도메인 규칙에서만 쓴다.
    "StatDefinition":    f"{ROOT}/Character/DT_StatDefinition",
    "LevelExp":          f"{ROOT}/Character/DT_LevelExp",
    "Enhance":           f"{ROOT}/Item/DT_Enhance",
    "MonsterDefinition": f"{ROOT}/Character/DT_MonsterDefinition",
}


# ── ① 참조 무결성 ────────────────────────────────────────────
# (테이블, 열, 가리키는 테이블, 빈 값을 허용하는가)
#
# 빈 값 허용은 "없어도 되는 참조" 를 뜻한다. SetId 처럼 세트에 속하지 않는 아이템이
# 정상인 경우다. 허용하지 않는 열이 비어 있으면 그 행 자체가 무의미하므로 오류로 본다.

REFERENCES = [
    ("ShopItem",     "ShopId",   "Shop",           False),
    ("ShopItem",     "ItemId",   "ItemDefinition", False),

    ("Skill",        "ClassId",  "CharacterClass", False),
    ("SkillEffect",  "SkillId",  "Skill",          False),
    ("SkillPassive", "SkillId",  "Skill",          False),

    ("ClassGrowth",  "ClassId",  "CharacterClass", False),
    ("UnionBonus",   "ClassId",  "CharacterClass", False),

    ("ItemStat",      "ItemId",  "ItemDefinition", False),
    ("ItemUseEffect", "ItemId",  "ItemDefinition", False),
    ("DropTable",     "ItemId",  "ItemDefinition", False),
    ("ItemSetBonus",  "SetId",   "ItemSet",        False),

    ("ItemDefinition", "SetId",  "ItemSet",        True),

    ("OptionPool",   "OptionId", "OptionDefinition", False),
]

# DT_ClassGrowth 의 ClassId 는 "모든 직업" 을 뜻하는 Default 를 쓸 수 있다.
# 코드(UTDProgressionComponent)가 그렇게 읽으므로 참조 검사에서도 예외로 둔다.
REFERENCE_EXCEPTIONS = {
    ("ClassGrowth", "ClassId"): {"Default"},
}


# ── ② enum 문자열 ────────────────────────────────────────────
# 오타가 나면 임포트가 조용히 첫 값을 넣는다. 오류도 경고도 없다.

ENUMS = [
    ("Skill", "SkillType",    {"Active", "Passive"}),
    ("Skill", "CastType",     {"Instant", "Cast", "Channel"}),
    ("Skill", "CastMovement", {"Free", "TurnOnly", "Locked"}),

    ("SkillEffect", "TargetTeam", {"Enemy", "Ally"}),

    ("ItemStat",     "Op", {"Base", "Added", "Increased", "More"}),
    ("SkillPassive", "Op", {"Base", "Added", "Increased", "More"}),
    ("ItemSetBonus", "Op", {"Base", "Added", "Increased", "More"}),
    ("SkillEffect",  "Op", {"Base", "Added", "Increased", "More"}),
    ("UnionBonus",   "Op", {"Base", "Added", "Increased", "More"}),
]


# ── ③ 커버리지 ───────────────────────────────────────────────
# 참조의 반대 방향이다. "가리키는 곳이 있는가" 가 아니라 "가리켜지고 있는가".
#
# (테이블, 필터열, 필터값, 대상테이블, 대상열, 설명)
# 필터열이 None 이면 전체 행이 대상이다.

COVERAGE = [
    ("Skill", "SkillType", "Active", "SkillEffect", "SkillId",
     "액티브인데 DT_SkillEffect 에 행이 없다 — 시전은 되는데 아무 일도 일어나지 않는다"),

    ("Skill", "SkillType", "Passive", "SkillPassive", "SkillId",
     "패시브인데 DT_SkillPassive 에 행이 없다 — 찍어도 스탯이 그대로다"),

    ("Shop", None, None, "ShopItem", "ShopId",
     "상점인데 파는 물건이 하나도 없다"),

    ("ItemSet", None, None, "ItemSetBonus", "SetId",
     "세트인데 단계 효과가 하나도 없다"),
]


# ═══════════════════════════════════════════════════════════════
#  아래는 규칙을 실행하는 부분이다. 규칙을 늘릴 때는 위만 고치면 된다.
# ═══════════════════════════════════════════════════════════════

class Report:
    """모아서 마지막에 한 번에 찍는다. 문제마다 따로 찍으면 순서가 섞여 읽기 어렵다."""

    def __init__(self):
        self.errors = []
        self.warnings = []

    def error(self, category, message):
        self.errors.append((category, message))

    def warn(self, category, message):
        self.warnings.append((category, message))

    def dump(self):
        unreal.log("")
        unreal.log("=" * 70)

        if not self.errors and not self.warnings:
            unreal.log("  데이터 검증 통과. 문제 없음.")
            unreal.log("=" * 70)
            return True

        for category, message in self.warnings:
            unreal.log_warning(f"  [경고] {category}: {message}")

        for category, message in self.errors:
            unreal.log_error(f"  [오류] {category}: {message}")

        unreal.log("-" * 70)
        unreal.log(f"  오류 {len(self.errors)}건, 경고 {len(self.warnings)}건")
        unreal.log("=" * 70)

        return len(self.errors) == 0


_loaded = {}


def load_table(alias, report):
    """별칭으로 테이블을 읽는다. 없으면 경고 한 번만 남기고 None."""
    if alias in _loaded:
        return _loaded[alias]

    path = TABLES.get(alias)
    table = unreal.EditorAssetLibrary.load_asset(path) if path else None

    if table is None:
        report.warn("테이블", f"'{alias}' 를 찾지 못했다 ({path}). 이 테이블 규칙은 건너뛴다.")

    _loaded[alias] = table
    return table


def row_names(table):
    return [str(name) for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]


def column(table, alias, column_name, report):
    """
    한 열을 문자열 목록으로 읽는다. 행 순서는 row_names 와 같다.

    타입별로 다르게 읽지 않는 이유는, 검증에 필요한 것이 "무엇을 가리키는가" 와
    "허용된 값인가" 뿐이어서 문자열 비교로 충분하기 때문이다.
    """
    values = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(table, column_name)
    values = [str(v) for v in values]

    names = row_names(table)

    # 열 이름이 틀리면 빈 목록이 온다. 그대로 두면 "검사할 것이 없다" 로 조용히 통과한다.
    if len(values) != len(names):
        report.error("스키마",
                     f"{alias} 에 '{column_name}' 열이 없다. 행 구조체가 바뀌었는지 확인할 것.")
        return None

    return list(zip(names, values))


def clean_enum(value):
    """ETDSkillType::Active 처럼 접두사가 붙어 오는 경우가 있어 뒤쪽만 남긴다."""
    return value.split("::")[-1].strip()


# ── 값 읽기 ───────────────────────────────────────────────────
# 열은 전부 문자열로 온다. 참조·enum 검사는 그대로 비교하면 됐지만
# 도메인 규칙은 숫자와 참·거짓을 봐야 해서 여기서 되돌린다.

TAG_RE = re.compile(r'^\(TagName="(.*)"\)$')
SCALABLE_VALUE_RE = re.compile(r'Value=(-?[\d.]+)')


def as_bool(value):
    return value.strip().lower() in ("true", "1")


def as_float(value, default=0.0):
    try:
        return float(value.strip())
    except ValueError:
        return default


def as_int(value, default=0):
    try:
        return int(float(value.strip()))
    except ValueError:
        return default


def as_tag(value):
    """(TagName="Item.Type.Accessory") → Item.Type.Accessory. 비어 있으면 빈 문자열."""
    text = value.strip()
    match = TAG_RE.match(text)
    if match:
        return match.group(1)

    # 태그가 비어 있으면 () 나 (TagName="") 로 온다. 오타 난 태그도 임포트 때 여기로 떨어진다.
    return "" if text in ("()", "", "None") else text


def as_scalable(value):
    """
    FScalableFloat 에서 기본값만 뽑는다. 커브가 붙어 있으면 그 값은 배수라
    절대 비교에는 쓸 수 없지만, 상하한이 뒤집혔는지 보는 데는 충분하다.
    읽지 못하면 None 을 돌려주고 검사를 건너뛴다.
    """
    match = SCALABLE_VALUE_RE.search(value)
    return float(match.group(1)) if match else None


def read_columns(table, alias, column_names, report):
    """
    여러 열을 한 번에 읽어 행마다 dict 로 묶는다.
    도메인 규칙은 대부분 같은 행의 두세 열을 함께 봐야 해서 이 형태가 편하다.

    하나라도 없으면 None — 그 규칙은 통째로 건너뛴다.
    """
    columns = {}
    for name in column_names:
        pairs = column(table, alias, name, report)
        if pairs is None:
            return None
        columns[name] = [value for _, value in pairs]

    names = row_names(table)

    rows = []
    for index, row_name in enumerate(names):
        entry = {"__name__": row_name}
        for name in column_names:
            entry[name] = columns[name][index]
        rows.append(entry)

    return rows


def check_references(report):
    for src_alias, column_name, dst_alias, allow_empty in REFERENCES:
        src = load_table(src_alias, report)
        dst = load_table(dst_alias, report)
        if src is None or dst is None:
            continue

        valid = set(row_names(dst))
        valid |= REFERENCE_EXCEPTIONS.get((src_alias, column_name), set())

        pairs = column(src, src_alias, column_name, report)
        if pairs is None:
            continue

        for row_name, value in pairs:
            value = value.strip()

            if not value or value == "None":
                if not allow_empty:
                    report.error("참조",
                                 f"{src_alias}[{row_name}] 의 {column_name} 가 비어 있다.")
                continue

            if value not in valid:
                report.error("참조",
                             f"{src_alias}[{row_name}] 의 {column_name}='{value}' 가 "
                             f"{dst_alias} 에 없다.")


def check_enums(report):
    for alias, column_name, allowed in ENUMS:
        table = load_table(alias, report)
        if table is None:
            continue

        pairs = column(table, alias, column_name, report)
        if pairs is None:
            continue

        for row_name, value in pairs:
            cleaned = clean_enum(value)
            if cleaned not in allowed:
                report.error("enum",
                             f"{alias}[{row_name}] 의 {column_name}='{value}' 는 허용값이 아니다. "
                             f"({' / '.join(sorted(allowed))})")


def check_coverage(report):
    for alias, filter_column, filter_value, dst_alias, dst_column, description in COVERAGE:
        src = load_table(alias, report)
        dst = load_table(dst_alias, report)
        if src is None or dst is None:
            continue

        referenced = column(dst, dst_alias, dst_column, report)
        if referenced is None:
            continue

        referenced_ids = {value.strip() for _, value in referenced}

        targets = row_names(src)

        if filter_column is not None:
            pairs = column(src, alias, filter_column, report)
            if pairs is None:
                continue
            targets = [name for name, value in pairs if clean_enum(value) == filter_value]

        for row_name in targets:
            if row_name not in referenced_ids:
                report.error("커버리지", f"{alias}[{row_name}] — {description}")


def check_skill_slots(report):
    """
    직업마다 액티브가 Q·W·E 세 자리를 하나씩 차지하는가.

    다른 규칙과 달리 두 열을 함께 봐야 해서 선언으로 표현할 수 없다.
    중복이면 한 키에 두 스킬이 걸려 뒤쪽이 영영 안 나오고, 빠지면 그 키가 먹통이 된다.
    """
    table = load_table("Skill", report)
    if table is None:
        return

    classes = column(table, "Skill", "ClassId", report)
    types = column(table, "Skill", "SkillType", report)
    slots = column(table, "Skill", "SlotIndex", report)

    if classes is None or types is None or slots is None:
        return

    # 직업 → 슬롯 번호 → 그 자리를 쓰는 스킬들
    by_class = {}

    for (row_name, class_id), (_, skill_type), (_, slot_text) in zip(classes, types, slots):
        if clean_enum(skill_type) != "Active":
            continue

        try:
            slot = int(float(slot_text))
        except ValueError:
            report.error("스킬 슬롯", f"Skill[{row_name}] 의 SlotIndex='{slot_text}' 를 숫자로 읽을 수 없다.")
            continue

        if slot not in (1, 2, 3):
            report.error("스킬 슬롯",
                         f"Skill[{row_name}] 은 액티브인데 SlotIndex={slot} 다. 1·2·3(Q·W·E) 이어야 한다.")
            continue

        by_class.setdefault(class_id.strip(), {}).setdefault(slot, []).append(row_name)

    for class_id, slot_map in sorted(by_class.items()):
        for slot, owners in sorted(slot_map.items()):
            if len(owners) > 1:
                report.error("스킬 슬롯",
                             f"직업 '{class_id}' 의 {slot}번 자리에 {len(owners)}개가 겹쳐 있다: "
                             f"{', '.join(owners)}")

        missing = [slot for slot in (1, 2, 3) if slot not in slot_map]
        if missing:
            # 만드는 중일 수 있으므로 경고다. 그 키를 눌러도 아무 일이 없을 뿐 깨지지는 않는다.
            report.warn("스킬 슬롯",
                        f"직업 '{class_id}' 에 {', '.join(str(s) for s in missing)}번 자리가 비어 있다.")


# ── ④ 도메인 규칙 ────────────────────────────────────────────
# 참조·enum 과 달리 선언으로 표현할 수 없다. "장신구이면서 스택 가능" 처럼
# 같은 행의 여러 열을 함께 봐야 하고, 규칙마다 보는 열이 다르기 때문이다.
#
# 여기 있는 대부분은 새로 정한 규칙이 아니라 **행 구조체 주석에 이미 적혀 있던 것**이다.
# 사람이 지키기로 한 규칙을 기계가 확인하게 옮겨 적은 것에 가깝다.

ACCESSORY_TAG = "Item.Type.Accessory"
QUEST_ITEM_TAG = "Item.Type.Quest"


def check_item_rules(report):
    table = load_table("ItemDefinition", report)
    if table is None:
        return

    rows = read_columns(table, "ItemDefinition",
                        ["ItemType", "bStackable", "MaxStackSize", "bCanDiscard", "Rarity",
                         "SellPrice"], report)
    if rows is None:
        return

    for row in rows:
        name = row["__name__"]
        item_type = as_tag(row["ItemType"])
        stackable = as_bool(row["bStackable"])

        if not item_type:
            report.error("아이템", f"[{name}] 의 ItemType 이 비어 있다. 태그 오타면 임포트가 조용히 비운다.")

        # FTDItemRow 주석이 이 검사를 직접 지목한다 —
        # 강화·추가옵션이 붙는 물건은 개체마다 달라 겹칠 수 없다.
        if item_type == ACCESSORY_TAG and stackable:
            report.error("아이템", f"[{name}] 은 장신구인데 bStackable=true 다. 장신구는 항상 false 여야 한다.")

        if not stackable and as_int(row["MaxStackSize"], 1) > 1:
            report.error("아이템",
                         f"[{name}] 은 bStackable=false 인데 MaxStackSize={row['MaxStackSize']} 다.")

        # 버릴 수 없는 것은 넘길 수도 없다. 거래소와 상점이 이 값 하나로 판단한다(D90).
        if item_type == QUEST_ITEM_TAG and as_bool(row["bCanDiscard"]):
            report.error("아이템", f"[{name}] 은 퀘스트 아이템인데 bCanDiscard=true 다.")

        if item_type == ACCESSORY_TAG and not as_tag(row["Rarity"]):
            report.error("아이템", f"[{name}] 은 장신구인데 Rarity 가 비어 있다. 추가옵션 풀을 거를 수 없다.")

        # SellPrice 0 은 "0 원에 팔린다" 는 뜻이지 오류가 아니다 — 잡템을 비우는 통로다.
        # 다만 장신구는 드롭 보상이라 0 이면 대개 값을 안 채운 것이다.
        # 소비 아이템까지 물으면 잡템마다 경고가 떠 목록이 소음이 된다.
        if item_type == ACCESSORY_TAG and as_int(row["SellPrice"]) <= 0:
            report.warn("아이템",
                        f"[{name}] 은 장신구인데 SellPrice 가 0 이다. "
                        f"팔면 0 원을 받는다 — 값을 안 채운 것인지 확인할 것.")


def check_stat_rules(report):
    table = load_table("StatDefinition", report)
    if table is None:
        return

    rows = read_columns(table, "StatDefinition",
                        ["StatTag", "DefaultValue", "bHasMinValue", "MinValue",
                         "bHasMaxValue", "MaxValue", "DisplayName",
                         "bIsPercent", "DecimalPlaces"], report)
    if rows is None:
        return

    for row in rows:
        name = row["__name__"]
        tag = as_tag(row["StatTag"])
        default = as_float(row["DefaultValue"])

        has_min = as_bool(row["bHasMinValue"])
        has_max = as_bool(row["bHasMaxValue"])
        minimum = as_float(row["MinValue"])
        maximum = as_float(row["MaxValue"])

        if not tag:
            report.error("스탯", f"[{name}] 의 StatTag 가 비어 있다.")
            continue

        if has_min and has_max and minimum > maximum:
            report.error("스탯", f"[{tag}] 의 MinValue({minimum}) 가 MaxValue({maximum}) 보다 크다.")

        # CritDamage 사고가 정확히 이것이었다 — Default 0.5 를 Min 1.0 이 끌어올려
        # 크리티컬이 터져도 피해가 그대로였다.
        if has_min and default < minimum:
            report.error("스탯",
                         f"[{tag}] 의 DefaultValue({default}) 가 MinValue({minimum}) 보다 작다. "
                         f"클램프에 끌려 올라가므로 적어둔 기본값이 실제로 쓰이지 않는다.")

        if has_max and default > maximum:
            report.error("스탯",
                         f"[{tag}] 의 DefaultValue({default}) 가 MaxValue({maximum}) 보다 크다.")

        # 받는 피해 감소는 1.0 이면 무적이다. 주석이 "반드시 상한을 건다" 고 못 박고 있다.
        if tag == "Stat.Defense.DamageReduction":
            if not has_max:
                report.error("스탯", f"[{tag}] 에 상한이 없다. 1.0 이 되면 무적이 된다.")
            elif maximum >= 1.0:
                report.error("스탯", f"[{tag}] 의 상한이 {maximum} 다. 1.0 미만이어야 한다.")

        # 상위 태그의 기본값은 계층을 타지 않아 하위 계산에 들어가지 않는다.
        # 적어둬도 아무 일이 없으므로 적은 사람이 오해한 것이다.
        if tag == "Stat.Offense.Damage" and default != 0.0:
            report.warn("스탯",
                        f"[{tag}] 는 상위 태그인데 DefaultValue={default} 다. "
                        f"기본값은 계층을 타지 않아 Physical/Magical 계산에 들어가지 않는다.")

        # ── 표시 ──
        # 셋 다 비어 있어도 게임은 돈다. 화면만 조용히 나빠진다.

        if not row["DisplayName"].strip():
            report.warn("스탯 표시",
                        f"[{tag}] 의 DisplayName 이 비어 있다. "
                        f"툴팁에 태그 문자열이 그대로 찍힌다.")

        # 0~1 비율인 스탯은 bIsPercent 가 꺼져 있으면 0.049 가 "0" 으로 보인다.
        # 값의 범위로 짐작한다 — 상한이 1 이하면 비율일 가능성이 높다.
        looks_like_ratio = has_max and 0.0 < maximum <= 1.0

        if looks_like_ratio and not as_bool(row["bIsPercent"]):
            report.warn("스탯 표시",
                        f"[{tag}] 는 상한이 {maximum} 인데 bIsPercent 가 꺼져 있다. "
                        f"0.049 가 '0' 으로 보인다. 비율 스탯이면 켤 것.")

        # 비율 스탯을 정수로 반올림하면 4.9% 가 5% 로 뭉개진다.
        if as_bool(row["bIsPercent"]) and as_int(row["DecimalPlaces"]) <= 0:
            report.warn("스탯 표시",
                        f"[{tag}] 는 퍼센트 표시인데 DecimalPlaces=0 이다. "
                        f"4.9% 가 5% 로 잘린다. 1 을 권한다.")


def check_curve_rules(report):
    table = load_table("LevelExp", report)
    if table is not None:
        rows = read_columns(table, "LevelExp", ["Level", "RequiredTotalExp"], report)
        if rows is not None:
            by_level = {}
            for row in rows:
                level = as_int(row["Level"])
                by_level.setdefault(level, []).append(row["__name__"])

            for level, owners in sorted(by_level.items()):
                if len(owners) > 1:
                    report.error("레벨 곡선",
                                 f"Level {level} 이 {len(owners)}행에 중복이다: {', '.join(owners)}")

            if 1 not in by_level:
                report.error("레벨 곡선", "Level 1 행이 없다.")

            # 요구 경험치가 레벨을 따라 늘지 않으면 GetLevelProgress 가 0 을 돌려준다 —
            # 경험치바가 차지 않는데 레벨은 오르는 상태가 된다.
            ordered = sorted((as_int(r["Level"]), as_int(r["RequiredTotalExp"]), r["__name__"])
                             for r in rows)
            for (prev_level, prev_exp, _), (level, exp, name) in zip(ordered, ordered[1:]):
                if exp <= prev_exp:
                    report.error("레벨 곡선",
                                 f"Level {level}({name}) 의 요구 경험치 {exp} 가 "
                                 f"Level {prev_level} 의 {prev_exp} 이하다. 단조 증가해야 한다.")

    table = load_table("Enhance", report)
    if table is None:
        return

    rows = read_columns(table, "Enhance",
                        ["Level", "DowngradeChanceOnFail", "MinDowngradeTiers", "MaxDowngradeTiers"],
                        report)
    if rows is None:
        return

    levels = sorted(as_int(row["Level"]) for row in rows)

    # 빠진 단계가 있으면 그 강화에서 조회가 실패해 더 올라가지 못한다.
    if levels:
        expected = list(range(1, max(levels) + 1))
        missing = sorted(set(expected) - set(levels))
        if missing:
            report.error("강화",
                         f"Level 이 연속이 아니다. 빠진 단계: {', '.join(str(m) for m in missing)}")

    for row in rows:
        name = row["__name__"]
        low = as_int(row["MinDowngradeTiers"])
        high = as_int(row["MaxDowngradeTiers"])

        if low > high:
            report.error("강화", f"[{name}] 의 MinDowngradeTiers({low}) 가 Max({high}) 보다 크다.")

        if as_float(row["DowngradeChanceOnFail"]) > 0.0 and high <= 0:
            report.warn("강화",
                        f"[{name}] 은 하락 확률이 있는데 MaxDowngradeTiers=0 이다. 실패해도 내려가지 않는다.")


def check_skill_rules(report):
    table = load_table("Skill", report)
    if table is None:
        return

    rows = read_columns(table, "Skill",
                        ["SkillType", "CastType", "ShapeTag", "Range", "Width",
                         "ChannelDuration", "ChannelInterval", "MaxLevel", "RequiredLevel",
                         "SlotIndex"], report)
    if rows is None:
        return

    max_character_level = get_max_character_level(report)

    for row in rows:
        name = row["__name__"]
        skill_type = clean_enum(row["SkillType"])

        if as_int(row["MaxLevel"]) <= 0:
            report.error("스킬", f"[{name}] 의 MaxLevel 이 0 이하다. 영영 찍을 수 없다.")

        required = as_int(row["RequiredLevel"])
        if max_character_level > 0 and required > max_character_level:
            report.error("스킬",
                         f"[{name}] 의 RequiredLevel({required}) 이 만렙({max_character_level}) 보다 높다. "
                         f"영영 찍을 수 없다.")

        if skill_type == "Passive":
            if as_int(row["SlotIndex"]) != 0:
                report.warn("스킬", f"[{name}] 은 패시브인데 SlotIndex={row['SlotIndex']} 다. 쓰이지 않는 값이다.")
            continue

        # ── 여기부터는 액티브만 ──
        cast_type = clean_enum(row["CastType"])
        shape = as_tag(row["ShapeTag"])
        skill_range = as_float(row["Range"])

        if not shape:
            report.error("스킬", f"[{name}] 은 액티브인데 ShapeTag 가 비어 있다. 대상을 찾지 못한다.")
        elif shape == "Skill.Shape.ForwardBox":
            if skill_range <= 0.0 or as_float(row["Width"]) <= 0.0:
                report.error("스킬",
                             f"[{name}] 은 ForwardBox 인데 Range={row['Range']}, Width={row['Width']} 다. "
                             f"상자 크기가 0 이라 아무도 맞지 않는다.")
        elif shape == "Skill.Shape.SelfRadius":
            if skill_range <= 0.0:
                report.error("스킬", f"[{name}] 은 SelfRadius 인데 Range 가 0 이다. 아무도 맞지 않는다.")

        duration = as_float(row["ChannelDuration"])
        interval = as_float(row["ChannelInterval"])

        if cast_type == "Channel":
            if duration <= 0.0 or interval <= 0.0:
                report.error("스킬",
                             f"[{name}] 은 정신집중인데 ChannelDuration={duration}, "
                             f"ChannelInterval={interval} 다. 반복이 돌지 않아 한 번만 나간다.")
        elif duration > 0.0 or interval > 0.0:
            report.warn("스킬",
                        f"[{name}] 은 {cast_type} 인데 Channel 값이 들어 있다. 쓰이지 않는 값이다.")


# ── 서식 인자 ────────────────────────────────────────────────
#
# 테이블의 문구에는 숫자가 없고 자리만 있다. 코드가 그 자리를 채우는데,
# 이름을 잘못 쓰면 **화면에 {Damage} 가 글자 그대로 남는다.** 오류도 경고도 없이,
# 그 스킬을 누가 툴팁으로 볼 때까지 아무도 모른다. 그래서 여기서 잡는다.
#
# 아래 목록은 UTDTooltipStatics::FormatSkillDescription 과 같아야 한다.

CAST_ARGUMENTS = {
    "Level", "Mana", "Cooldown", "CastTime",
    "Duration", "Interval", "Ticks", "Range", "Width",
}

FORMAT_ARG_RE = re.compile(r"\{([A-Za-z_][A-Za-z0-9_]*)\}")
ORDERED_ARG_RE = re.compile(r"\{(\d+)\}")


def tag_argument_names(tag, prefix):
    """
    태그 하나가 만드는 인자 이름들. UTDTooltipStatics::AddTagArgument 와 같은 규칙이다.

      Stat.Defense.Armor  →  {Defense_Armor} 와 {Armor}
      Skill.Effect.Damage →  {Damage}          (조각이 하나라 둘이 같다)
    """
    if not tag:
        return set()

    name = tag[len(prefix):] if tag.startswith(prefix) else tag
    return {name.replace(".", "_"), name.rsplit(".", 1)[-1]}


def collect_by_skill(alias, tag_column, prefix, report):
    """스킬마다 쓸 수 있는 인자 이름을 모은다. {SkillId: {이름, ...}}"""
    table = load_table(alias, report)
    if table is None:
        return {}

    rows = read_columns(table, alias, ["SkillId", tag_column], report)
    if rows is None:
        return {}

    result = {}
    for row in rows:
        skill = row["SkillId"].strip()
        result.setdefault(skill, set()).update(
            tag_argument_names(as_tag(row[tag_column]), prefix))

    return result


def collect_effect_arguments(report):
    """
    액티브 효과가 만드는 인자 이름. 위와 달리 행마다 어느 열을 보는지가 갈린다.

    **버프는 EffectTag 가 아니라 StatTag 를 따른다.** 효과 태그로 이름을 만들면
    한 스킬의 버프가 여럿일 때 전부 {Buff} 가 되어 서로 덮어쓴다 — 마법사의
    전열 강화가 물리·마법을 따로 올리는 것이 그 경우다.
    UTDTooltipStatics::FormatSkillDescription 이 같은 규칙으로 갈린다.
    """
    table = load_table("SkillEffect", report)
    if table is None:
        return {}

    rows = read_columns(table, "SkillEffect", ["SkillId", "EffectTag", "StatTag"], report)
    if rows is None:
        return {}

    result = {}
    for row in rows:
        skill = row["SkillId"].strip()
        tag = as_tag(row["EffectTag"])

        if tag == "Skill.Effect.Buff":
            names = tag_argument_names(as_tag(row["StatTag"]), "Stat.")
        else:
            names = tag_argument_names(tag, "Skill.Effect.")

        result.setdefault(skill, set()).update(names)

    return result


def check_format_arguments(report):
    table = load_table("Skill", report)
    if table is None:
        return

    rows = read_columns(table, "Skill", ["Description", "SkillType"], report)
    if rows is None:
        return

    effects = collect_effect_arguments(report)
    passives = collect_by_skill("SkillPassive", "StatTag", "Stat.", report)

    for row in rows:
        name = row["__name__"]
        description = row["Description"]

        if not description.strip():
            report.warn("설명", f"[{name}] 의 Description 이 비어 있다. 툴팁에 수치가 하나도 안 나온다.")
            continue

        # 값에서 나온 이름과 시전 이름을 합친 것이 이 스킬이 쓸 수 있는 전부다.
        value_arguments = effects.get(name, set()) | passives.get(name, set())
        allowed = CAST_ARGUMENTS | value_arguments

        for used in FORMAT_ARG_RE.findall(description):
            if used not in allowed:
                report.error("설명",
                             f"[{name}] 의 Description 이 {{{used}}} 를 쓰는데 채울 값이 없다. "
                             f"화면에 글자 그대로 남는다. 쓸 수 있는 이름: "
                             f"{', '.join(sorted(allowed))}")

        # 순서 인자는 스킬에서 쓰지 않는다. {0} 은 아무것도 채우지 않는다.
        for used in ORDERED_ARG_RE.findall(description):
            report.error("설명",
                         f"[{name}] 의 Description 이 {{{used}}} 를 쓴다. 스킬은 이름 인자만 쓴다.")

        # 값 인자는 이미 `%` 가 붙어서 나온다. 문장에 또 쓰면 "150%%" 가 된다.
        for used, _ in re.findall(r"\{([A-Za-z_][A-Za-z0-9_]*)\}(%)", description):
            if used in value_arguments:
                report.error("설명",
                             f"[{name}] 의 Description 이 {{{used}}}% 로 되어 있다. "
                             f"이 값은 `%` 를 스스로 붙이므로 문장에서는 빼야 한다.")


def check_option_rules(report):
    table = load_table("OptionDefinition", report)
    if table is None:
        return

    rows = read_columns(table, "OptionDefinition",
                        ["DisplayName", "StatTag", "MinValue", "MaxValue"], report)
    if rows is None:
        return

    for row in rows:
        name = row["__name__"]

        if not as_tag(row["StatTag"]):
            report.error("옵션", f"[{name}] 의 StatTag 가 비어 있다. 붙어도 스탯이 오르지 않는다.")

        low = as_float(row["MinValue"])
        high = as_float(row["MaxValue"])
        if low > high:
            report.error("옵션", f"[{name}] 의 MinValue({low}) 가 MaxValue({high}) 보다 크다.")

        if high == 0.0:
            report.warn("옵션", f"[{name}] 의 MaxValue 가 0 이다. 붙어도 아무 효과가 없다.")

        # 옵션은 값이 언제나 하나라 순서 인자를 쓴다. 자리가 없으면 수치가 안 보인다.
        display = row["DisplayName"]
        if ORDERED_ARG_RE.findall(display) != ["0"]:
            report.error("옵션",
                         f"[{name}] 의 DisplayName 에 {{0}} 이 정확히 하나 있어야 한다: \"{display}\"")

    # 아이템이 가리키는 풀이 실제로 있는가. RowName 이 아니라 열 값끼리라
    # 위쪽 REFERENCES 로는 볼 수 없다.
    pool = load_table("OptionPool", report)
    items = load_table("ItemDefinition", report)
    if pool is None or items is None:
        return

    pool_ids = column(pool, "OptionPool", "PoolId", report)
    item_rows = read_columns(items, "ItemDefinition", ["OptionPoolId", "ItemType"], report)
    if pool_ids is None or item_rows is None:
        return

    known = {value.strip() for _, value in pool_ids if value.strip()}

    for row in item_rows:
        item_name = row["__name__"]
        pool_id = row["OptionPoolId"].strip()

        # 빈 FName 은 에디터가 'None' 이라는 글자로 넘긴다. 포션처럼 옵션이 없는 아이템은
        # 풀을 비워 두는 게 정상이므로 "없는 풀" 로 잡으면 안 된다.
        #
        # 다만 장신구는 예외다. 얻는 순간 AddItem 이 첫 옵션을 굴리는데, 풀이 비어 있으면
        # 오류도 경고도 없이 0줄로 들어오고 재굴림도 막힌다.
        if pool_id in ("", "None"):
            if as_tag(row["ItemType"]) == "Item.Type.Accessory":
                report.error("옵션",
                             f"[{item_name}] 은 장신구인데 OptionPoolId 가 비어 있다. "
                             f"옵션 없이 들어오고 재굴림도 할 수 없다.")
            continue

        if pool_id not in known:
            report.error("옵션",
                         f"[{item_name}] 의 OptionPoolId '{pool_id}' 가 DT_OptionPool 에 없다. "
                         f"굴려도 옵션이 하나도 붙지 않는다.")


# ── 툴팁이 읽는 테이블 ───────────────────────────────────────
#
# UTDUISettings 의 참조가 비어 있으면 툴팁이 **조용히 덜 나온다.** 오류도 경고도 없이
# 스탯 이름이 "Stat.Offense.Damage.Physical" 처럼 태그 그대로 찍히고, 소수 자릿수와
# 퍼센트 표시도 기본값으로 떨어진다. 화면을 보기 전에는 알 수 없다.

TOOLTIP_SETTINGS = [
    ("TooltipStatDefinitionTable", "스탯 이름·퍼센트 여부·소수 자릿수"),
    ("TooltipItemStatTable", "아이템이 주는 고정 스탯"),
    ("TooltipItemTable", "아이템 정의 (인벤토리 밖에서 여는 툴팁)"),
    ("TooltipUseEffectTable", "소비 아이템 효과"),
]


def check_tooltip_settings(report):
    path = unreal.Paths.combine([unreal.Paths.project_config_dir(), "DefaultGame.ini"])

    try:
        with open(path, "r", encoding="utf-8") as handle:
            config = handle.read()
    except OSError:
        report.warn("툴팁 설정", "DefaultGame.ini 를 읽지 못했다. 이 검사는 건너뛴다.")
        return

    for key, purpose in TOOLTIP_SETTINGS:
        if re.search(rf"^{key}\s*=\s*\S", config, re.MULTILINE) is None:
            report.warn("툴팁 설정",
                        f"UTDUISettings 의 {key} 이 비어 있다 ({purpose}). "
                        f"프로젝트 세팅 > Game > TD UI 에서 지정할 것.")


# ── 게임 규칙이 읽는 테이블 ──────────────────────────────────
#
# 툴팁과 달리 이쪽은 비어 있으면 **기능이 통째로 멈춘다.** 그래서 경고가 아니라 오류다.
#
# dev 머지(b2cc8b6, 2026-09-11)에서 TDItemOptionSettings 섹션이 통째로 사라진 적이 있다.
# 한쪽만 가진 섹션이라 git 은 충돌로 보지 않았고, 사흘 뒤 "장신구에 옵션이 안 붙고
# 재굴림 비용이 0" 으로 드러났다. 머지할 때마다 같은 일이 생길 수 있어 여기서 잡는다.
#
#   (키, 프로젝트 세팅 위치, 비었을 때 일어나는 일)
GAMEPLAY_TABLE_SETTINGS = [
    ("OptionRarityTable", "TD > Item Option", "추가 옵션을 굴리지도 재굴림하지도 못한다"),
    ("OptionPoolTable",   "TD > Item Option", "추가 옵션을 굴리지도 재굴림하지도 못한다"),
    ("DropTable",         "TD > Drop",        "어느 몬스터도 아이템을 떨구지 않는다"),
    ("UnionBonusTable",   "TD > Character",   "유니온 보너스가 아무에게도 붙지 않는다"),
]


def check_gameplay_table_settings(report):
    path = unreal.Paths.combine([unreal.Paths.project_config_dir(), "DefaultGame.ini"])

    try:
        with open(path, "r", encoding="utf-8") as handle:
            config = handle.read()
    except OSError:
        report.warn("설정", "DefaultGame.ini 를 읽지 못했다. 이 검사는 건너뛴다.")
        return

    for key, location, consequence in GAMEPLAY_TABLE_SETTINGS:
        if re.search(rf"^{key}\s*=\s*\S", config, re.MULTILINE) is None:
            report.error("설정",
                         f"{key} 가 DefaultGame.ini 에 없다 — {consequence}. "
                         f"프로젝트 세팅 > {location} 에서 지정할 것. "
                         f"dev 머지 뒤라면 섹션이 통째로 사라졌을 수 있다.")


# ── 코드에 박힌 만렙 ─────────────────────────────────────────
#
# 강화 상승폭과 재굴림 비용은 "아이템 착용레벨 / 만렙" 으로 보간한다. 그 만렙이
# C++ 상수로 박혀 있다 — 두 함수를 순수하게(월드도 컴포넌트도 없이) 두려고 한 선택이라
# 그 자체는 의도한 것이다. **문제는 DT_LevelExp 와 어긋나도 아무도 모른다는 점이다.**
#
# 만렙을 60 으로 올리면 50레벨 장비가 최고 배율을 받아 버리고, 그 위 구간은 전부
# 같은 값이 된다. 조용히 틀리는 종류라 여기서 잡는다.

MAX_LEVEL_CONSTANTS = [
    ("Source/TD_Project/Items/TDEnhanceStatics.cpp", "강화 1강당 상승폭"),
    ("Source/TD_Project/Items/TDItemOptionStatics.cpp", "재굴림 비용 배율"),
]

REFERENCE_MAX_LEVEL_RE = re.compile(r"ReferenceMaxLevel\s*=\s*([\d.]+)f?\s*;")


def check_reference_max_level(report):
    expected = get_max_character_level(report)
    if expected <= 0:
        return

    project_dir = unreal.Paths.project_dir()

    for relative_path, purpose in MAX_LEVEL_CONSTANTS:
        path = unreal.Paths.combine([project_dir, relative_path])

        try:
            with open(path, "r", encoding="utf-8") as handle:
                source = handle.read()
        except OSError:
            report.warn("만렙 상수", f"{relative_path} 를 읽지 못했다. 이 검사는 건너뛴다.")
            continue

        match = REFERENCE_MAX_LEVEL_RE.search(source)
        if match is None:
            report.warn("만렙 상수",
                        f"{relative_path} 에서 ReferenceMaxLevel 을 찾지 못했다. "
                        f"이름이 바뀌었다면 ValidateData.py 의 정규식도 함께 고칠 것.")
            continue

        found = int(float(match.group(1)))
        if found != expected:
            report.error("만렙 상수",
                         f"{relative_path} 의 ReferenceMaxLevel={found} 인데 "
                         f"DT_LevelExp 의 만렙은 {expected} 다 ({purpose}). "
                         f"보간 기준이 어긋나 고레벨 장비가 제 값을 못 받는다.")


def check_skill_effect_rules(report):
    table = load_table("SkillEffect", report)
    if table is None:
        return

    rows = read_columns(table, "SkillEffect",
                        ["SkillId", "EffectTag", "BaseValue", "TargetTeam",
                         "Duration", "StatTag"], report)
    if rows is None:
        return

    for row in rows:
        name = row["__name__"]
        tag = as_tag(row["EffectTag"])
        duration = as_float(row["Duration"])
        stat = as_tag(row["StatTag"])
        team = clean_enum(row["TargetTeam"])

        if tag == "Skill.Effect.Buff":
            # 셋 중 하나만 빠져도 버프가 조용히 아무 일도 하지 않는다.
            if duration <= 0.0:
                report.error("스킬 효과",
                             f"[{name}] 은 버프인데 Duration 이 0 이다. 걸자마자 걷힌다.")
            if not stat:
                report.error("스킬 효과",
                             f"[{name}] 은 버프인데 StatTag 가 비어 있다. 올릴 스탯이 없다.")
            if as_float(row["BaseValue"]) == 0.0:
                report.warn("스킬 효과",
                            f"[{name}] 은 버프인데 BaseValue 가 0 이다. 1레벨에서 효과가 없다.")

        elif tag == "Skill.Effect.Invulnerable":
            if duration <= 0.0:
                report.error("스킬 효과",
                             f"[{name}] 은 무적인데 Duration 이 0 이다. 아무 일도 일어나지 않는다.")

        else:
            # 즉발 효과가 지속 열을 채우고 있으면 적은 사람이 오해한 것이다.
            if duration > 0.0:
                report.warn("스킬 효과",
                            f"[{name}] 은 즉발({tag})인데 Duration={duration} 이 들어 있다. 쓰이지 않는 값이다.")
            if stat:
                report.warn("스킬 효과",
                            f"[{name}] 은 즉발({tag})인데 StatTag 가 들어 있다. 쓰이지 않는 값이다.")

        # 회복을 적에게 주면 적을 살린다. 반대로 피해를 아군에게 주면 팀킬이다.
        if tag in ("Skill.Effect.Heal", "Skill.Effect.RestoreMana") and team == "Enemy":
            report.error("스킬 효과",
                         f"[{name}] 은 회복인데 TargetTeam=Enemy 다. 적을 회복시킨다.")

        if tag == "Skill.Effect.Damage" and team == "Ally":
            report.warn("스킬 효과",
                        f"[{name}] 은 피해인데 TargetTeam=Ally 다. 아군을 때린다 — 의도한 것인지 확인할 것.")


def get_max_character_level(report):
    """DT_LevelExp 의 마지막 레벨. 못 읽으면 0 — 그러면 관련 검사를 건너뛴다."""
    table = load_table("LevelExp", report)
    if table is None:
        return 0

    pairs = column(table, "LevelExp", "Level", report)
    if not pairs:
        return 0

    return max(as_int(value) for _, value in pairs)


def check_shop_rules(report):
    table = load_table("ShopItem", report)
    if table is None:
        return

    rows = read_columns(table, "ShopItem", ["ShopId", "ItemId", "Price"], report)
    if rows is None:
        return

    settings_rate = 0.25  # UTDShopSettings 의 기본값. 상점별 상향은 여기서 보지 않는다.
    seen = {}

    for row in rows:
        name = row["__name__"]
        price = as_int(row["Price"])
        key = (row["ShopId"].strip(), row["ItemId"].strip())

        if price <= 0:
            report.error("상점", f"[{name}] 의 Price 가 {price} 다. 공짜로 팔린다.")

        if key in seen:
            report.error("상점",
                         f"[{name}] 과 [{seen[key]}] 이 같은 (상점, 아이템) 을 두 번 정의한다. "
                         f"어느 가격이 쓰일지 알 수 없다.")
        else:
            seen[key] = name

    items = load_table("ItemDefinition", report)
    if items is None:
        return

    item_rows = read_columns(items, "ItemDefinition", ["bCanDiscard", "SellPrice"], report)
    if item_rows is None:
        return

    no_discard = {row["__name__"] for row in item_rows if not as_bool(row["bCanDiscard"])}
    sell_price = {row["__name__"]: as_int(row["SellPrice"]) for row in item_rows}

    for row in rows:
        name = row["__name__"]
        item_id = row["ItemId"].strip()
        price = as_int(row["Price"])

        # 파는 물건인데 되팔 수 없는 경우.
        # bCanDiscard 가 "버릴 수 있는가" 와 "넘길 수 있는가" 를 겸하므로(D90),
        # false 인 물건은 팔 수 없다. 데이터가 깨진 것은 아니라 확인만 요청한다.
        if item_id in no_discard:
            report.warn("상점",
                        f"[{name}] 이 파는 '{item_id}' 는 bCanDiscard=false 라 되팔 수 없다. "
                        f"사기만 되고 팔지는 못한다.")

        # 무한 골드. 사서 되파는 것만으로 돈이 늘어난다.
        #
        # 구매가와 판매가를 따로 적게 되면서 생긴 구멍이다. 되팔기를 구매가에 묶었을
        # 때는 비율이 1 미만인 한 구조적으로 불가능했다.
        gain = round(sell_price.get(item_id, 0) * settings_rate)
        if price > 0 and gain >= price:
            report.error("상점",
                         f"[{name}] — '{item_id}' 를 {price} 에 사서 {gain} 에 되팔 수 있다. "
                         f"사고파는 것만으로 골드가 늘어난다.")


def check_monster_rules(report):
    table = load_table("MonsterDefinition", report)
    if table is None:
        return

    rows = read_columns(table, "MonsterDefinition", ["DamageType", "GoldMin", "GoldMax"], report)
    if rows is None:
        return

    allowed = {"", "Stat.Offense.Damage.Physical", "Stat.Offense.Damage.Magical"}

    for row in rows:
        name = row["__name__"]

        # 상위 태그(Stat.Offense.Damage)에 넣으면 기본값이 계층을 타지 않아
        # 하위 계산에 반영되지 않는다. 비워두는 것은 허용된다 — 코드가 물리로 취급한다.
        damage_type = as_tag(row["DamageType"])
        if damage_type not in allowed:
            report.error("몬스터",
                         f"[{name}] 의 DamageType='{damage_type}' 는 쓸 수 없다. "
                         f"Physical 이나 Magical 을 지정하거나 비워둘 것.")

        low = as_scalable(row["GoldMin"])
        high = as_scalable(row["GoldMax"])
        if low is not None and high is not None and low > high:
            report.warn("몬스터",
                        f"[{name}] 의 GoldMin({low}) 이 GoldMax({high}) 보다 크다. "
                        f"코드가 뒤집어 처리하지만 시트가 틀린 것이다.")


def check_drop_rules(report):
    """
    드롭 목록. 참조가 RowName 이 아니라 열(DropTableId)이라 ① 의 형식으로는 못 잡는다.

    한 목록에 여러 행이 속하므로 DropTableId 는 RowName 이 될 수 없다.
    그래서 "몬스터가 가리키는 목록이 실제로 있는가" 를 여기서 따로 본다.
    """
    drop_table = load_table("DropTable", report)
    monster_table = load_table("MonsterDefinition", report)
    item_table = load_table("ItemDefinition", report)

    if drop_table is None or monster_table is None or item_table is None:
        return

    drops = read_columns(drop_table, "DropTable",
                         ["DropTableId", "ItemId", "Chance", "MinCount", "MaxCount"], report)
    monsters = read_columns(monster_table, "MonsterDefinition", ["DropTableId"], report)
    items = read_columns(item_table, "ItemDefinition", ["bStackable"], report)

    if drops is None or monsters is None or items is None:
        return

    stackable = {row["__name__"]: as_bool(row["bStackable"]) for row in items}

    known_lists = {row["DropTableId"] for row in drops if row["DropTableId"]}
    used_lists = set()

    for row in monsters:
        list_id = row["DropTableId"]

        # 비어 있으면 아이템을 떨구지 않는다. 경험치·골드는 그대로 나가므로 정상이다.
        if not list_id or list_id == "None":
            continue

        used_lists.add(list_id)

        if list_id not in known_lists:
            report.error("드롭",
                         f"몬스터 [{row['__name__']}] 의 DropTableId='{list_id}' 가 "
                         f"DT_DropTable 에 없다. 아무것도 떨구지 않는다.")

    for list_id in sorted(known_lists - used_lists):
        report.warn("드롭",
                    f"목록 '{list_id}' 를 쓰는 몬스터가 없다. "
                    f"DT_MonsterDefinition 의 DropTableId 를 채우지 않은 것은 아닌지 확인할 것.")

    for row in drops:
        name = row["__name__"]
        item_id = row["ItemId"]

        chance = as_float(row["Chance"])
        if chance <= 0.0:
            report.warn("드롭", f"[{name}] 의 Chance 가 {chance} 다. 영영 나오지 않는다.")
        elif chance > 1.0:
            report.warn("드롭",
                        f"[{name}] 의 Chance 가 {chance} 다. 0~1 사이여야 하며 "
                        f"1 을 넘겨도 확률이 더 오르지 않는다.")

        low = as_int(row["MinCount"], 1)
        high = as_int(row["MaxCount"], 1)

        if low < 1:
            report.error("드롭", f"[{name}] 의 MinCount 가 {low} 다. 1 이상이어야 한다.")

        if low > high:
            report.error("드롭", f"[{name}] 의 MinCount({low}) 가 MaxCount({high}) 보다 크다.")

        # 겹치지 않는 아이템이 한 번에 둘 이상 들어오면 칸을 그만큼 먹는다.
        # 장신구 두 개가 동시에 들어와 인벤토리가 꽉 차면 뒤엣것이 조용히 사라진다.
        if item_id in stackable and not stackable[item_id] and high > 1:
            report.error("드롭",
                         f"[{name}] 의 '{item_id}' 는 겹치지 않는 아이템인데 MaxCount={high} 다. "
                         f"1 이어야 한다.")


def main():
    unreal.log("")
    unreal.log("데이터 검증을 시작한다...")

    report = Report()

    check_references(report)
    check_enums(report)
    check_coverage(report)
    check_skill_slots(report)

    check_item_rules(report)
    check_stat_rules(report)
    check_curve_rules(report)
    check_skill_rules(report)
    check_skill_effect_rules(report)
    check_shop_rules(report)
    check_monster_rules(report)
    check_drop_rules(report)
    check_format_arguments(report)
    check_option_rules(report)
    check_reference_max_level(report)
    check_tooltip_settings(report)
    check_gameplay_table_settings(report)

    if not report.dump():
        # -run=pythonscript 는 예외로 끝나야 종료 코드가 0 이 아니게 된다.
        # CI 가 실패를 알아채려면 이 줄이 필요하다.
        raise RuntimeError(f"데이터 검증 실패: 오류 {len(report.errors)}건")


main()
