#!/usr/bin/env python3
"""画 D7 的"线程数 → 帧时间"曲线（不依赖 matplotlib，只用 Pillow）。

用法：
    ./build/bench --frames 30 --csv docs/bench_d7.csv
    python3 tools/plot_threads.py docs/bench_d7.csv docs/images/d7_threads.png
"""

import csv
import os
import sys

from PIL import Image, ImageDraw, ImageFont

FONT_CANDIDATES = [
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
    "/System/Library/Fonts/STHeiti Light.ttc",
    "/Library/Fonts/Arial Unicode.ttf",
]
COLORS = [(64, 120, 200), (220, 130, 60), (90, 160, 90)]


def load_font(size):
    for path in FONT_CANDIDATES:
        if os.path.exists(path):
            try:
                return ImageFont.truetype(path, size)
            except OSError:
                continue
    return ImageFont.load_default(size=size)


def load(path):
    """返回 {分辨率: [(线程数, ms), ...]}，按线程数排好序。"""
    curves = {}
    with open(path, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            v = r["variant"]
            if v == "incremental":
                n = 1
            elif v.startswith("tiled-"):
                n = int(r["threads"])
            else:
                continue
            res = f"{r['width']}x{r['height']}"
            curves.setdefault(res, []).append((n, float(r["ms_median"])))
    for res in curves:
        curves[res].sort()
    return curves


def panel(d, box, title, curves, font, small, as_speedup):
    left, top, right, bottom = box
    d.text((left, top - 30), title, font=font, fill=(30, 30, 30))

    xs = sorted({n for c in curves.values() for n, _ in c})
    ys = []
    for res, pts in curves.items():
        base = pts[0][1]
        for _, ms in pts:
            ys.append(base / ms if as_speedup else ms)
    ymax = max(ys) * 1.15
    ymin = 0.0

    def px(n):
        return left + (right - left) * (n - xs[0]) / max(1, (xs[-1] - xs[0]))

    def py(v):
        return bottom - (bottom - top) * (v - ymin) / (ymax - ymin)

    # 网格
    for i in range(5):
        v = ymin + (ymax - ymin) * i / 4
        y = py(v)
        d.line([(left, y), (right, y)], fill=(225, 225, 223))
        d.text((left - 8, y - 7), f"{v:.1f}", font=small, fill=(60, 60, 60), anchor="ra")

    # 理想线性（仅加速比面板）
    if as_speedup:
        # 理想线性：n 线程 n 倍。超出坐标范围的部分就不画了
        x_end = xs[0]
        for n in xs:
            if float(n) <= ymax:
                x_end = n
        d.line([(px(xs[0]), py(1.0)), (px(x_end), py(float(x_end)))], fill=(185, 185, 185), width=2)
        d.text((px(x_end) + 6, py(float(x_end)) - 6), "理想线性 n 倍", font=small,
               fill=(150, 150, 150))

    for i, (res, pts) in enumerate(sorted(curves.items())):
        color = COLORS[i % len(COLORS)]
        base = pts[0][1]
        xy = [(px(n), py(base / ms if as_speedup else ms)) for n, ms in pts]
        d.line(xy, fill=color, width=3)
        for idx, ((x, y), (n, ms)) in enumerate(zip(xy, pts)):
            d.ellipse([x - 4, y - 4, x + 4, y + 4], fill=color)
            # 加速比面板只标首尾：4 线程处三条线贴得太近，全标会糊成一团
            if as_speedup and 0 < idx < len(pts) - 1:
                continue
            label = f"{base / ms:.2f}x" if as_speedup else f"{ms:.1f}"
            d.text((x, y - 16), label, font=small, fill=color, anchor="ma")

    for n in xs:
        d.text((px(n), bottom + 8), str(n), font=small, fill=(60, 60, 60), anchor="ma")
    d.text(((left + right) / 2, bottom + 28), "线程数", font=small, fill=(60, 60, 60), anchor="ma")
    d.line([(left, bottom), (right, bottom)], fill=(40, 40, 40))


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "docs/bench_d7.csv"
    dst = sys.argv[2] if len(sys.argv) > 2 else "docs/images/d7_threads.png"
    curves = load(src)

    W, H = 1180, 560
    img = Image.new("RGB", (W, H), (250, 250, 248))
    d = ImageDraw.Draw(img)
    font = load_font(16)
    small = load_font(13)
    big = load_font(19)

    d.text((60, 14), "D7 tile 多线程：线程数 → 帧时间（中位数）", font=big, fill=(30, 30, 30))
    panel(d, (70, 80, 500, 450), "每帧耗时（ms，越低越好）", curves, font, small, False)
    panel(d, (690, 80, 1120, 450), "相对单线程的加速比（×）", curves, font, small, True)

    # 图例
    lx = 70
    for i, res in enumerate(sorted(curves)):
        d.rectangle([lx, H - 44, lx + 14, H - 30], fill=COLORS[i % len(COLORS)])
        d.text((lx + 20, H - 46), res, font=small, fill=(40, 40, 40))
        lx += 110

    img.save(dst)
    print(f"已写出 {dst} ({W}x{H})")


if __name__ == "__main__":
    main()
