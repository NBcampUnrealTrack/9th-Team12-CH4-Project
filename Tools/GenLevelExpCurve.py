# DT_LevelExp 용 곡선을 만든다.
#
# 구간 요구량 = BASE * n^EXPONENT  (n = 현재 레벨)
# 테이블에는 그것을 누적한 값을 넣는다 — 레벨이 경험치로부터 계산되는 파생값이기 때문.
#
# 초반은 빠르게(몇 마리로 레벨업), 후반은 완만하게(증가율이 서서히 둔화).
#
# 밸런스를 조정할 때는 아래 세 숫자만 바꿔 다시 돌리고, 나온 CSV 를 구글 시트의
# LevelExp 탭에 덮어쓴 뒤 GDA_LevelExp 에서 로드하면 된다.
#
#   EXPONENT 를 올리면  후반이 가팔라진다 (레벨당 요구량 증가폭이 커짐)
#   BASE 를 올리면      전 구간이 똑같은 비율로 늘어난다
#
# 실행:
#   "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" GenLevelExpCurve.py

import csv
import os

MAX_LEVEL = 50
BASE = 80
EXPONENT = 1.55

OUT = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "Content", "Data", "DataTables", "Character", "CSV", "GoogleSheet", "DT_LevelExp.csv")


def round_nice(value):
    """자릿수에 맞춰 보기 좋게 자른다. 기획이 시트에서 읽고 고치기 쉬우라고."""
    if value < 1000:
        return int(round(value / 10.0)) * 10
    if value < 10000:
        return int(round(value / 100.0)) * 100
    return int(round(value / 1000.0)) * 1000


rows = [["Name", "Level", "RequiredTotalExp"]]

total = 0
for level in range(1, MAX_LEVEL + 1):
    rows.append(["Lv%02d" % level, level, total])

    # 다음 레벨까지의 구간 요구량
    span = round_nice(BASE * (level ** EXPONENT))
    total += span

os.makedirs(os.path.dirname(OUT), exist_ok=True)

# BOM 을 붙인다(utf-8-sig). ConvertExportedCsv.py 와 같은 이유 —
# 없으면 엑셀이 시스템 코드페이지로 읽어 한글이 깨진다.
with open(OUT, "w", encoding="utf-8-sig", newline="") as f:
    csv.writer(f).writerows(rows)

print("생성: %s\n" % OUT)

# 감각을 확인할 수 있게 몇 구간만 찍는다.
print("%-6s %12s %12s" % ("레벨", "누적", "이번 구간"))
prev = 0
for row in rows[1:]:
    level, cumulative = row[1], row[2]
    if level in (1, 2, 3, 5, 10, 20, 30, 40, 50):
        print("%-6d %12s %12s" % (level, format(cumulative, ","), format(cumulative - prev, ",")))
    prev = cumulative
