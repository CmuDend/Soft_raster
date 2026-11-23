#!/usr/bin/env python3
"""把 bench 输出的 CSV 画成柱状图（不依赖 matplotlib，只用 Pillow）。

用法：
    ./build/bench --frames 30 --csv docs/bench_d6.csv
    python3 tools/plot_bench.py docs/bench_d6.csv docs/images/d6_bench.png
"""

import csv
import os
import sys

from PIL import Image, ImageDraw, ImageFont

# macOS 自带的中文字体：load_default() 没有 CJK 字形，会画成一串方框
FONT_CANDIDATES = [
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
    "/System/Library/Fonts/STHeiti Light.ttc",
    "/Library/Fonts/Arial Unicode.ttf",
    "/System/Library/Fonts/Supplemental/Songti.ttc",
]


def load_font(size):
    for path in FONT_CANDIDATES:
        if os.path.exists(path):
            try:
                return ImageFont.truetype(path, size)
            except OSError:
                continue
    return ImageFont.load_default(size=size)

W, H = 1180, 600
BG = (250, 250, 248)
AXIS = (40, 40, 40)
GRID = (222, 222, 220)

# 三种 variant 的配色
COLORS = {
    "naive": (150, 120, 200),
    "incremental": (60, 140, 200),
    "no-cull": (230, 160, 70),
    "flat-shade": (200, 200, 190),
}
LABELS = {
    "naive": "naive（逐像素边函数）",
    "incremental": "incremental（增量边函数）",
    "no-cull": "no-cull（关背面剔除）",
    "flat-shade": "flat-shade（常量着色）",
}


def load(path):
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            rows.append(
                {
                    "res": f"{r['width']}x{r['height']}",
                    "variant": r["variant"],
                    "ms": float(r["ms_median"]),
                    "vertex": float(r["ms_vertex"]),
                    "raster": float(r["ms_raster"]),
                    "tris": int(r["triangles"]),
                    "overdraw": float(r["overdraw"]),
                }
            )
    return rows


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "docs/bench_d6.csv"
    dst = sys.argv[2] if len(sys.argv) > 2 else "docs/images/d6_bench.png"
    rows = load(src)

    res_order = []
    for r in rows:
        if r["res"] not in res_order:
            res_order.append(r["res"])
    variants = [v for v in ("naive", "incremental", "no-cull", "flat-shade")
                if any(r["variant"] == v for r in rows)]

    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    font = load_font(15)
    small = load_font(13)
    big = load_font(19)

    left, right, top, bottom = 78, W - 40, 46, H - 92
    d.text((left, 14), "D6 基准：每帧耗时（中位数，30 帧，丢 3 帧预热）", font=big, fill=AXIS)

    values = [r["ms"] for r in rows]
    ymax = max(values) * 1.18

    # y 轴网格
    steps = 5
    for i in range(steps + 1):
        y = bottom - (bottom - top) * i / steps
        d.line([(left, y), (right, y)], fill=GRID)
        d.text((left - 10, y - 7), f"{ymax * i / steps:4.0f}", font=small, fill=AXIS, anchor="ra")
    d.text((14, top - 24), "ms/帧", font=small, fill=AXIS)

    group_w = (right - left) / len(res_order)
    bar_w = min(46.0, group_w / (len(variants) + 1.2))

    for gi, res in enumerate(res_order):
        gx = left + group_w * gi
        d.text((gx + group_w / 2, bottom + 10), res, font=font, fill=AXIS, anchor="ma")
        for vi, variant in enumerate(variants):
            row = next((r for r in rows if r["res"] == res and r["variant"] == variant), None)
            if row is None:
                continue
            x0 = gx + group_w / 2 + (vi - len(variants) / 2) * bar_w
            hgt = (bottom - top) * row["ms"] / ymax
            d.rectangle([x0 + 2, bottom - hgt, x0 + bar_w - 2, bottom], fill=COLORS[variant])
            d.text((x0 + bar_w / 2, bottom - hgt - 15), f"{row['ms']:.1f}", font=small,
                   fill=AXIS, anchor="ma")

        # 增量边函数相对 naive 的提速标注
        naive = next((r["ms"] for r in rows if r["res"] == res and r["variant"] == "naive"), None)
        inc = next((r["ms"] for r in rows if r["res"] == res and r["variant"] == "incremental"), None)
        if naive and inc:
            d.text((gx + group_w / 2, bottom + 30), f"增量 -{(1 - inc / naive) * 100:.1f}%",
                   font=small, fill=(60, 120, 60), anchor="ma")

    d.line([(left, bottom), (right, bottom)], fill=AXIS)

    # 图例
    lx = left
    for variant in variants:
        d.rectangle([lx, H - 34, lx + 14, H - 20], fill=COLORS[variant])
        d.text((lx + 20, H - 36), LABELS.get(variant, variant), font=small, fill=AXIS)
        lx += 20 + int(d.textlength(LABELS.get(variant, variant), font=small)) + 22

    img.save(dst)
    print(f"已写出 {dst} ({W}x{H})")


if __name__ == "__main__":
    main()
