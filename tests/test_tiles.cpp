#include "test_framework.h"

#include <cstring>
#include <vector>

#include "core/framebuffer.h"
#include "pipeline/tiles.h"

namespace {

struct FlatShader : IShader {
    Color color = 0;
    Color fragment(const Fragment&) const override { return color; }
};

// 顶点顺序按"屏幕空间 CCW"给：(x,y) → (x,y+h) → (x+w,y)。
// 屏幕 y 向下，所以这个顺序算出来 area > 0，是正面（不会被背面剔除踢掉）。
ScreenTri makeTri(float x, float y, float w, float h, const IShader* shader) {
    ScreenTri t;
    t.a.x = x;
    t.a.y = y;
    t.b.x = x;
    t.b.y = y + h;
    t.c.x = x + w;
    t.c.y = y;
    t.a.z = t.b.z = t.c.z = 0.0f;
    t.a.invW = t.b.invW = t.c.invW = 1.0f;
    t.shader = shader;
    return t;
}

// 不渲染，只检查分箱的"账"对不对：每个 tile 拿到的三角形数加起来应该等于
// 所有三角形覆盖的 tile 数之和，而且每条记录都得能对上号
void testBinCounts() {
    tf::caseName("分箱：offsets 与 indices 自洽");

    FlatShader shader;
    std::vector<ScreenTri> tris;
    tris.push_back(makeTri(0, 0, 20, 20, &shader));      // 全在 tile(0,0)
    tris.push_back(makeTri(30, 30, 40, 40, &shader));    // 跨 4 个 tile
    tris.push_back(makeTri(-100, -100, 5, 5, &shader));  // 完全在屏幕外

    const TileLists tiles = binTriangles(tris, 128, 128, 32);
    CHECK(tiles.cols == 4);
    CHECK(tiles.rows == 4);
    CHECK(tiles.tileCount() == 16);

    // offsets 单调不减，且最后一项等于 indices 的长度
    bool monotonic = true;
    for (size_t i = 1; i < tiles.offsets.size(); ++i)
        if (tiles.offsets[i] < tiles.offsets[i - 1]) monotonic = false;
    CHECK(monotonic);
    CHECK(tiles.offsets.back() == int(tiles.indices.size()));

    // 按三角形统计"被登记到几个 tile"
    int hits[3] = {0, 0, 0};
    for (int k : tiles.indices) ++hits[k];

    CHECK(hits[0] == 1);  // (0,0)-(20,20)：只落在 tile(0,0)
    CHECK(hits[1] == 9);  // (30,30)-(70,70)：x/y 各跨 3 个 tile
    CHECK(hits[2] == 0);  // 屏幕外：根本不登记
}

// tile 边界：一个正好从 x=32 开始的三角形，像素 31 属于左 tile、32 属于右 tile，
// 两边都得画到，而且不能重复画（重复会浪费一次深度测试，也可能写出不同的 z）
void testTileBoundary() {
    tf::caseName("tile 边界：交界像素既不漏也不重");

    FlatShader shader;
    shader.color = rgb(255, 0, 0);

    std::vector<ScreenTri> tris;
    // 覆盖 [0,64) × [0,64) 的直角三角形
    tris.push_back(makeTri(0, 0, 64, 64, &shader));

    const int size = 128;
    const TileLists tiles = binTriangles(tris, size, size, 32);

    Framebuffer a(size, size), b(size, size);
    a.clear(0, 1.0f);
    b.clear(0, 1.0f);

    RasterOptions opt;
    RasterCounters c1, c2;
    renderTiles(a, tris, tiles, opt, 1, &c1);
    renderTiles(b, tris, tiles, opt, 4, &c2);

    // 多线程与单线程必须逐字节一致：写集合不相交，结果就不该依赖线程数
    CHECK(std::memcmp(a.colorData(), b.colorData(), sizeof(Color) * size_t(size) * size_t(size)) == 0);
    CHECK(c1.covered == c2.covered);

    // 交界像素：x=31 和 x=32 在 y=1 那一行都该被盖住
    CHECK(a.colorData()[size_t(1) * size + 31] == rgb(255, 0, 0));
    CHECK(a.colorData()[size_t(1) * size + 32] == rgb(255, 0, 0));

    // 三角形斜边之外（右上角）应该是空的
    CHECK(a.colorData()[size_t(1) * size + 120] == 0);
}

}  // namespace

void testTiles() {
    testBinCounts();
    testTileBoundary();
}
