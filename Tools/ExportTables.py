# DataTable 을 CSV 로 뽑는다.
#
# 언리얼의 Export 는 에디터에서 우클릭 > Asset Actions > Export 로 하나씩 하는 수동 작업이다.
# 밸런스를 볼 때마다 대여섯 개를 우클릭하게 되어 스크립트로 옮겼다.
#
# 뽑은 원본은 사람이 읽는 형식이 아니다 — NSLOCTEXT(...), (TagName="..."), 10.000000.
# 그 정리는 이미 ConvertExportedCsv.py 가 하므로 여기서는 뽑기만 하고,
# 이어 붙이는 것은 ExportTables.bat 이 한다.
#
# 사용법:
#   ExportTables.bat 을 더블클릭 (에디터가 열려 있으면 실패한다)
#
# 결과: Content/Data/DataTables/<도메인>/CSV/DT_Xxx.csv

import os

import unreal


ROOT = "/Game/Data/DataTables"

# (에셋 경로, Content/Data/DataTables 아래 어느 폴더에 둘 것인가)
#
# 생성기가 만드는 테이블은 넣지 않는다. DT_Option*/DT_Enhance/DT_Skill*/DT_LevelExp 는
# CSV 가 진실의 원천이고 .uasset 은 그것을 임포트한 결과다. 거꾸로 뽑으면
# 원본을 덮어쓰면서, 임포트를 빠뜨린 차이가 조용히 사라진다.
TABLES = [
    (f"{ROOT}/Character/DT_ClassGrowth",       "Character/CSV"),
    (f"{ROOT}/Character/DT_StatDefinition",    "Character/CSV"),
    (f"{ROOT}/Character/DT_MonsterDefinition", "Character/CSV"),
    (f"{ROOT}/Item/DT_ItemDefinition",         "Item/CSV"),
    (f"{ROOT}/Item/DT_ItemStat",               "Item/CSV"),
    (f"{ROOT}/Item/DT_ItemSet",                "Item/CSV"),
    (f"{ROOT}/Item/DT_ItemSetBonus",           "Item/CSV"),
    (f"{ROOT}/Item/DT_ItemUseEffect",          "Item/CSV"),
]


def main():
    base = os.path.join(unreal.Paths.project_content_dir(), "Data", "DataTables")

    exported = 0
    for asset_path, sub_dir in TABLES:
        table = unreal.load_asset(asset_path)
        if table is None:
            unreal.log_error("테이블이 없다: %s" % asset_path)
            continue

        out_dir = os.path.abspath(os.path.join(base, sub_dir))
        os.makedirs(out_dir, exist_ok=True)

        out_path = os.path.join(out_dir, "%s.csv" % asset_path.rsplit("/", 1)[-1])

        if unreal.DataTableFunctionLibrary.export_data_table_to_csv_file(table, out_path):
            unreal.log("  %s" % out_path)
            exported += 1
        else:
            unreal.log_error("내보내기 실패: %s" % asset_path)

    unreal.log("테이블 %d/%d 개를 내보냈다." % (exported, len(TABLES)))


main()
