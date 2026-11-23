#include "test_framework.h"

#include "pipeline/clip.h"

namespace {

VSOut makeClipVert(float x, float y, float z, float w) {
    VSOut v;
    v.clip = {x, y, z, w};
    v.worldPos = {x, y, z};
    v.normal = {0.0f, 1.0f, 0.0f};
    v.uv = {0.0f, 0.0f};
    return v;
}

// nearDist = z + w：1.5 在内，-1.5 在外
VSOut inside(float x = 0.0f) { return makeClipVert(x, 0.0f, 0.5f, 1.0f); }
VSOut outside(float x = 0.0f) { return makeClipVert(x, 0.0f, -0.5f, -1.0f); }

}  // namespace

static void testAllInside() {
    TF_CASE("裁剪：三个顶点都在近平面内 → 原样输出 3 个");
    const VSOut in[3] = {inside(1.0f), inside(2.0f), inside(3.0f)};
    VSOut out[4];

    const int n = clipTriangleNear(in, out);
    CHECK(n == 3);
    for (int i = 0; i < 3; ++i) {
        CHECK_NEAR(out[i].clip.x, in[i].clip.x, 1e-6f);
        CHECK_NEAR(nearDist(out[i]), nearDist(in[i]), 1e-6f);
    }
}

static void testOneOutside() {
    TF_CASE("裁剪：一个顶点在外 → 4 个顶点，交点正好落在近平面上");
    const VSOut in[3] = {inside(1.0f), outside(3.0f), inside(5.0f)};
    VSOut out[4];

    const int n = clipTriangleNear(in, out);
    CHECK(n == 4);

    int intersections = 0;
    for (int i = 0; i < n; ++i) {
        CHECK(nearDist(out[i]) >= -1e-5f);  // 不允许有顶点留在近平面外
        if (std::fabs(nearDist(out[i])) <= 1e-5f) ++intersections;
    }
    CHECK(intersections == 2);

    // in[0]=(x=1, dist=1.5) 与 in[1]=(x=3, dist=-1.5) 距离对称 → t=0.5 → x=2
    bool foundX2 = false;
    for (int i = 0; i < n; ++i)
        if (std::fabs(out[i].clip.x - 2.0f) < 1e-5f) foundX2 = true;
    CHECK(foundX2);
}

static void testTwoOutside() {
    TF_CASE("裁剪：两个顶点在外 → 3 个顶点");
    const VSOut in[3] = {outside(1.0f), outside(3.0f), inside(5.0f)};
    VSOut out[4];

    const int n = clipTriangleNear(in, out);
    CHECK(n == 3);
    for (int i = 0; i < n; ++i) CHECK(nearDist(out[i]) >= -1e-5f);
}

static void testAllOutside() {
    TF_CASE("裁剪：三个顶点都在外 → 0 个，整片丢弃");
    const VSOut in[3] = {outside(1.0f), outside(2.0f), outside(3.0f)};
    VSOut out[4];

    CHECK(clipTriangleNear(in, out) == 0);
}

static void testAttributesInterpolated() {
    TF_CASE("裁剪：交点上的 uv / worldPos 按齐次坐标线性插值");
    VSOut a = inside(0.0f);
    a.uv = {0.0f, 0.0f};
    a.worldPos = {0.0f, 0.0f, 0.0f};
    VSOut b = outside(0.0f);
    b.uv = {1.0f, 2.0f};
    b.worldPos = {4.0f, 8.0f, 12.0f};

    const VSOut in[3] = {a, b, inside(9.0f)};
    VSOut out[4];
    CHECK(clipTriangleNear(in, out) == 4);

    // 找 a→b 那条边的交点：a 与 b 的 clip.x 都是 0，所以交点 x 也是 0
    // （b→c 那条边的交点 x = 4.5，用 x 来区分，避免 uv 恰好也相等）
    bool found = false;
    for (int i = 0; i < 4; ++i) {
        if (std::fabs(out[i].clip.x) < 1e-5f && std::fabs(nearDist(out[i])) < 1e-5f) {
            found = true;
            CHECK_NEAR(out[i].uv.y, 1.0f, 1e-5f);
            CHECK_NEAR(out[i].worldPos.x, 2.0f, 1e-5f);
            CHECK_NEAR(out[i].worldPos.z, 6.0f, 1e-5f);
        }
    }
    CHECK(found);
}

void testClip() {
    testAllInside();
    testOneOutside();
    testTwoOutside();
    testAllOutside();
    testAttributesInterpolated();
}
