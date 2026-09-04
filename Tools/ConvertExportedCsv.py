# 언리얼이 내보낸 DataTable CSV 를 구글 시트에 넣기 좋은 형태로 바꾼다.
#
# 언리얼의 Export 는 사람이 읽으라고 만든 형식이 아니다.
#
#   ---                          -> Name
#   (TagName="Stat.Xxx")         -> Stat.Xxx
#   NSLOCTEXT("k1","k2","이름")   -> 이름
#   10.000000                    -> 10
#   빈 NewRow 행                  -> 삭제
#   UTF-16 (한글이 섞이면)         -> UTF-8
#
# 사용법:
#   ConvertCsvForSheet.bat 을 더블클릭하거나
#   python ConvertExportedCsv.py <CSV 가 있는 폴더>
#
# 결과는 그 폴더 아래 GoogleSheet\ 에 만들어진다. 원본은 건드리지 않는다.
#
# 주의: 이 스크립트는 "기존 테이블을 시트로 처음 옮길 때" 쓰는 도구다.
#       시트를 진실의 원천으로 삼은 뒤에는 반대 방향(시트 -> DataTable)만 쓰게 되므로
#       평소에는 실행할 일이 없다.

import csv
import io
import os
import re
import sys

TAG_RE = re.compile(r'^\(TagName="(.*)"\)$', re.S)
LOC_RE = re.compile(r'^NSLOCTEXT\(\s*".*?"\s*,\s*".*?"\s*,\s*"(.*)"\s*\)$', re.S)
NUM_RE = re.compile(r'^-?\d+\.\d+$')

OUT_DIR_NAME = "GoogleSheet"


def read_rows(path):
    with open(path, "rb") as f:
        raw = f.read()

    # 언리얼은 한글이 섞이면 UTF-16 으로 내보낸다.
    if raw[:2] in (b"\xff\xfe", b"\xfe\xff"):
        text = raw.decode("utf-16")
    else:
        text = raw.decode("utf-8-sig")

    return list(csv.reader(io.StringIO(text)))


def clean(value):
    v = value.strip()

    m = TAG_RE.match(v)
    if m:
        return m.group(1)

    m = LOC_RE.match(v)
    if m:
        return m.group(1)

    if NUM_RE.match(v):
        f = float(v)
        return str(int(f)) if f == int(f) else ("%g" % f)

    return v


def is_placeholder(row):
    """
    에디터에서 추가만 하고 안 채운 행.

    컬럼 이름으로 찾으면 Rarity 처럼 Tag 로 끝나지 않는 태그를 놓치므로
    빈 태그 값 자체를 본다.
    """
    if not row or row[0] != "NewRow":
        return False

    return any(cell.strip() == '(TagName="")' for cell in row[1:])


def convert(src_dir, name):
    src = os.path.join(src_dir, name)
    rows = read_rows(src)
    if not rows:
        print("  건너뜀 (빈 파일): %s" % name)
        return

    header = ["Name"] + [clean(c) for c in rows[0][1:]]

    out = [header]
    dropped = 0
    for row in rows[1:]:
        if not row or not row[0].strip():
            continue
        if is_placeholder(row):
            dropped += 1
            continue
        out.append([clean(c) for c in row])

    out_dir = os.path.join(src_dir, OUT_DIR_NAME)
    os.makedirs(out_dir, exist_ok=True)

    # BOM 을 붙인다(utf-8-sig). 없으면 엑셀이 시스템 코드페이지로 읽어서
    # 한글이 깨진 채로 열리고, 그 상태로 저장하면 그대로 굳는다.
    with open(os.path.join(out_dir, name), "w", encoding="utf-8-sig", newline="") as f:
        csv.writer(f).writerows(out)

    note = " (빈 행 %d개 제거)" % dropped if dropped else ""
    print("  %-28s %d행%s" % (name, len(out) - 1, note))


def main():
    if len(sys.argv) > 1:
        src_dir = sys.argv[1]
    else:
        src_dir = input("CSV 가 있는 폴더 경로: ").strip('" ')

    if not os.path.isdir(src_dir):
        print("폴더를 찾을 수 없다: %s" % src_dir)
        return 1

    names = [n for n in sorted(os.listdir(src_dir)) if n.lower().endswith(".csv")]
    if not names:
        print("CSV 파일이 없다: %s" % src_dir)
        return 1

    print("변환 시작 -> %s\n" % os.path.join(src_dir, OUT_DIR_NAME))

    for name in names:
        convert(src_dir, name)

    print("\n완료. 구글 시트에서 파일 > 가져오기 > 업로드 로 넣으면 된다.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
