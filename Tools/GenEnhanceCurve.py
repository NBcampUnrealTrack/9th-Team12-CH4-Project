# DT_Enhance 용 확률표를 만든다.
#
# 세 구간이다.
#   1~6강    하락 없음. 성공률만 낮아진다
#   7~12강   실패 시 하락(1단계 고정)이 생긴다. 6강 끝에서 성공률이 한 번 다시 오른 뒤
#            다시 낮아진다 — 하락 위험이 생기는 대신 진입 문턱을 낮춘 것이다
#   13~18강  실패 시 하락이 랜덤 1~3단계. 성공률·하락확률은 **7~12강을 그대로 반복**한다
#            (13강 = 7강, 14강 = 8강, ...) — 하락 폭만 넓어진다
#
# 아래 배열의 길이가 곧 각 구간의 레벨 수(6)다. 구간을 늘리거나 줄이려면
# BAND_A_SUCCESS / BAND_B_SUCCESS / BAND_B_DOWNGRADE_CHANCE 의 길이를 맞춰서 고친다.

import csv
import io
import os

BAND_A_SUCCESS = [0.95, 0.90, 0.83, 0.75, 0.65, 0.55]   # 1~6강. 하락 없음
BAND_B_SUCCESS = [0.70, 0.62, 0.54, 0.46, 0.38, 0.30]   # 7~12강 (= 13~18강)
BAND_B_DOWNGRADE_CHANCE = [0.20, 0.25, 0.30, 0.35, 0.40, 0.45]   # 실패했을 때 하락으로 이어질 확률

BASE_COST = 100
COST_EXPONENT = 1.8   # 레벨이 오를수록 비용이 가파르게 오른다

OUT = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "Content", "Data", "DataTables", "Item", "CSV", "GoogleSheet", "DT_Enhance.csv")


def round_nice(value):
    if value < 1000:
        return int(round(value / 10.0)) * 10
    return int(round(value / 100.0)) * 100


rows = [["Name", "Level", "SuccessRate", "DowngradeChanceOnFail",
         "MinDowngradeTiers", "MaxDowngradeTiers", "Cost"]]

# 1~6강 — 하락 없음
for i, success in enumerate(BAND_A_SUCCESS):
    level = i + 1
    cost = round_nice(BASE_COST * (level ** COST_EXPONENT))
    rows.append(["Lv%02d" % level, level, success, 0.0, 0, 0, cost])

# 7~12강 — 하락 1단계 고정
for i, (success, downgrade) in enumerate(zip(BAND_B_SUCCESS, BAND_B_DOWNGRADE_CHANCE)):
    level = len(BAND_A_SUCCESS) + i + 1
    cost = round_nice(BASE_COST * (level ** COST_EXPONENT))
    rows.append(["Lv%02d" % level, level, success, downgrade, 1, 1, cost])

# 13~18강 — 7~12강을 그대로 반복. 하락만 1~3단계로 넓어진다
for i, (success, downgrade) in enumerate(zip(BAND_B_SUCCESS, BAND_B_DOWNGRADE_CHANCE)):
    level = len(BAND_A_SUCCESS) + len(BAND_B_SUCCESS) + i + 1
    cost = round_nice(BASE_COST * (level ** COST_EXPONENT))
    rows.append(["Lv%02d" % level, level, success, downgrade, 1, 3, cost])

os.makedirs(os.path.dirname(OUT), exist_ok=True)

# BOM 을 붙인다(utf-8-sig). 없으면 엑셀이 시스템 코드페이지로 읽어 한글이 깨진다.
with io.open(OUT, "w", encoding="utf-8-sig", newline="") as f:
    csv.writer(f).writerows(rows)

print("생성: %s (%d행, 최대 %d강)\n" % (OUT, len(rows) - 1, rows[-1][1]))

print("%-4s %8s %10s %6s" % ("Lv", "성공률", "실패시하락", "비용"))
for row in rows[1:]:
    level, success, downgrade, cost = row[1], row[2], row[3], row[6]
    print("%-4d %7.0f%% %9.0f%% %6d" % (level, success * 100, downgrade * 100, cost))
