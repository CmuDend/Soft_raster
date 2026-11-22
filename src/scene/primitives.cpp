#include "scene/mesh.h"

#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

}  // namespace

// 立方体：棱长 2、中心在原点、CCW 朝外。
// 每个面单独 4 个顶点 —— 法线不能跨面共享，否则立方体边缘会被"磨圆"。
Mesh makeCube() {
    const Vec3 n[6] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    const Vec3 p[6][4] = {
        {{-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}},  // +Z 前
        {{ 1, -1, -1}, {-1, -1, -1}, {-1,  1, -1}, { 1,  1, -1}},  // -Z 后
        {{ 1, -1,  1}, { 1, -1, -1}, { 1,  1, -1}, { 1,  1,  1}},  // +X 右
        {{-1, -1, -1}, {-1, -1,  1}, {-1,  1,  1}, {-1,  1, -1}},  // -X 左
        {{-1,  1,  1}, { 1,  1,  1}, { 1,  1, -1}, {-1,  1, -1}},  // +Y 上
        {{-1, -1, -1}, { 1, -1, -1}, { 1, -1,  1}, {-1, -1,  1}},  // -Y 下
    };
    // 每面 uv 只占 [0,0.5]²，这样每个面铺 4×4 格（整张贴图是 8×8 格）
    const Vec2 uv[4] = {{0, 0}, {0.5f, 0}, {0.5f, 0.5f}, {0, 0.5f}};

    Mesh m;
    for (int f = 0; f < 6; ++f) {
        const int base = int(m.verts.size());
        for (int i = 0; i < 4; ++i) m.verts.push_back({p[f][i], n[f], uv[i]});
        m.indices.insert(m.indices.end(), {base + 0, base + 1, base + 2,
                                           base + 0, base + 2, base + 3});
    }
    return m;
}

// 地面：y = y 的水平网格，边长 2*half，切成 cells×cells 块，法线朝上。
// uv 随 half 一起放大，保证不管地面多大，一格棋盘都是 1 个世界单位。
Mesh makeFloor(float y, float half, int cells) {
    const float uvMax = half * 0.25f;
    const int stride = cells + 1;

    Mesh m;
    m.verts.reserve(size_t(stride) * size_t(stride));
    for (int j = 0; j <= cells; ++j) {
        const float fz = -half + 2.0f * half * float(j) / float(cells);
        for (int i = 0; i <= cells; ++i) {
            const float fx = -half + 2.0f * half * float(i) / float(cells);
            m.verts.push_back({{fx, y, fz},
                               {0, 1, 0},
                               {uvMax * float(i) / float(cells), uvMax * float(j) / float(cells)}});
        }
    }
    for (int j = 0; j < cells; ++j) {
        for (int i = 0; i < cells; ++i) {
            const int a = j * stride + i;   // (i,   j)
            const int b = a + 1;            // (i+1, j)
            const int c = a + stride + 1;   // (i+1, j+1)
            const int d = a + stride;       // (i,   j+1)
            m.indices.insert(m.indices.end(), {a, c, b, a, d, c});   // 正面（area > 0）
        }
    }
    return m;
}

// UV 球：法线就是单位化后的位置，所以表面是"光滑"的 —— 高光才能连成一片
Mesh makeSphere(float radius, int rings, int segs) {
    Mesh m;
    for (int r = 0; r <= rings; ++r) {
        const float phi = kPi * float(r) / float(rings);   // 0=北极 .. π=南极
        for (int s = 0; s <= segs; ++s) {
            const float th = 2.0f * kPi * float(s) / float(segs);
            const Vec3 n{std::sin(phi) * std::cos(th), std::cos(phi), std::sin(phi) * std::sin(th)};
            m.verts.push_back({n * radius, n, {float(s) / float(segs), float(r) / float(rings)}});
        }
    }
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segs; ++s) {
            const int a = r * (segs + 1) + s;
            const int b = a + segs + 1;
            m.indices.insert(m.indices.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    }
    return m;
}
