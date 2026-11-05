#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_template_tokens.py

扫描模板 Excel（.xlsx）里用到的字段占位符，转换模板库前先做一次体检。

用法:
    python tools/check_template_tokens.py <目录或文件> [更多路径...]

输出:
    - 每个文件/sheet 用到的占位符及次数
    - 不认识的 #xxx（会原样显示在表格里，需要修正）
"""
import sys
import re
from pathlib import Path
from collections import Counter

TOKENS = {
    "projectName", "projectCode", "ownerUnit", "constructionUnit", "designUnit", "supervisionUnit",
    "unitName", "subName", "itemName", "sheetName", "date",
}
PATTERN = re.compile(r"#([A-Za-z][A-Za-z0-9]*)")


def iter_xlsx(paths):
    for raw in paths:
        p = Path(raw)
        if p.is_dir():
            yield from sorted(p.rglob("*.xlsx"))
        elif p.suffix.lower() == ".xlsx":
            yield p


def scan_workbook(path):
    from openpyxl import load_workbook

    wb = load_workbook(path, read_only=True, data_only=True)
    for ws in wb.worksheets:
        counts = Counter()
        unknown = Counter()
        for row in ws.iter_rows():
            for cell in row:
                if not isinstance(cell.value, str):
                    continue
                for key in PATTERN.findall(cell.value):
                    if key in TOKENS:
                        counts[key] += 1
                    else:
                        unknown[key] += 1
        if counts or unknown:
            yield ws.title, counts, unknown
    wb.close()


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1

    files = list(iter_xlsx(argv[1:]))
    if not files:
        print("没有找到 .xlsx 文件（.xls 需要先另存为 .xlsx）")
        return 1

    total_known = 0
    total_unknown = 0

    for path in files:
        try:
            sheets = list(scan_workbook(path))
        except Exception as exc:  # noqa: BLE001
            print(f"[跳过] {path}: {exc}")
            continue
        if not sheets:
            continue

        print(f"\n{path}")
        for title, counts, unknown in sheets:
            used = ", ".join(f"#{k}×{v}" for k, v in sorted(counts.items()))
            print(f"  [{title}] {used or '(无)'}")
            if unknown:
                bad = ", ".join(f"#{k}×{v}" for k, v in sorted(unknown.items()))
                print(f"    !! 未知占位符: {bad}")
                total_unknown += sum(unknown.values())
            total_known += sum(counts.values())

    print(f"\n合计: 已知占位符 {total_known} 处，未知 {total_unknown} 处")
    if total_unknown:
        print("请按 docs/模板字段标记说明.md 修正未知占位符后再重新转换。")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
