#!/usr/bin/env python3
"""生成演示用的 OBJ 模型（圆环）。

存在意义：D4 的加载器需要一个"真实文件"来验证，而不是只有程序化几何。
参数化圆环的好处是法线、UV、绕序都能算准，出问题一定出在加载器而不是模型。

用法： python3 tools/gen_obj.py            # 写入 assets/torus.obj
"""
import math
import os

R = 0.85      # 环心半径
r = 0.33      # 管半径
NU = 56       # 绕大圈的分段
NV = 26       # 绕管截面的分段


def main() -> None:
    out = ["# 参数化圆环，由 tools/gen_obj.py 生成",
           f"# R={R} r={r} nu={NU} nv={NV}"]
    verts, norms, uvs, faces = [], [], [], []

    for i in range(NU):
        u = 2.0 * math.pi * i / NU
        cu, su = math.cos(u), math.sin(u)
        for j in range(NV):
            v = 2.0 * math.pi * j / NV
            cv, sv = math.cos(v), math.sin(v)
            # 环面：大圈半径 R+ 小圈 r*cos(v)，截面在 (径向, Y) 平面里
            nx, ny, nz = cv * cu, sv, cv * su
            verts.append(((R + r * cv) * cu, r * sv, (R + r * cv) * su))
            norms.append((nx, ny, nz))
            uvs.append((i / NU, j / NV))

    for i in range(NU):
        i1 = (i + 1) % NU
        for j in range(NV):
            j1 = (j + 1) % NV
            a = i * NV + j
            b = i1 * NV + j
            c = i1 * NV + j1
            d = i * NV + j1
            faces.append((a, c, b))
            faces.append((a, d, c))

    for p in verts:
        out.append(f"v {p[0]:.6f} {p[1]:.6f} {p[2]:.6f}")
    for t in uvs:
        out.append(f"vt {t[0]:.6f} {t[1]:.6f}")
    for n in norms:
        out.append(f"vn {n[0]:.6f} {n[1]:.6f} {n[2]:.6f}")
    for f in faces:
        # OBJ 的索引是 1-based，且 v/vt/vn 三组共用同一个序号
        out.append(f"f {f[0]+1}/{f[0]+1}/{f[0]+1} {f[1]+1}/{f[1]+1}/{f[1]+1} "
                   f"{f[2]+1}/{f[2]+1}/{f[2]+1}")

    path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                        "assets", "torus.obj")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fp:
        fp.write("\n".join(out) + "\n")
    print(f"{path}: {len(verts)} verts, {len(faces)} tris")


if __name__ == "__main__":
    main()
