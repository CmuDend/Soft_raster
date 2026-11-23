#include "test_framework.h"

#include "pipeline/rasterizer.h"

namespace {

// 只返回一个常量颜色，把光栅化器和着色逻辑解耦
struct FlatShader : IShader {
    Color color = 0;
    Color fragment(const Fragment&) const override { return color; }
};

SVert makeVert(float x, float y, float ndcZ) {
    SVert v;
    v.x = x;
    v.y = y;
    v.z = ndcZ;
    v.invW = 1.0f;
    return v;
}

// 覆盖整个 32×32 画面的三角形
void drawCover(Framebuffer& fb, float ndcZ, Color color) {
    FlatShader shader;
    shader.color = color;
    // 斜边 x+y=64 必须落在 32×32 画面的外侧，才能只用一个三角形铺满全屏
    drawTriangle(fb, makeVert(-1.0f, -1.0f, ndcZ), makeVert(65.0f, -1.0f, ndcZ),
                 makeVert(-1.0f, 65.0f, ndcZ), RasterOptions{false, true, true}, shader);
}

int countColor(const Framebuffer& fb, Color c) {
    int n = 0;
    const Color* data = fb.colorData();
    for (int i = 0; i < fb.width() * fb.height(); ++i)
        if (data[i] == c) ++n;
    return n;
}

}  // namespace

static void testEdgeSign() {
    TF_CASE("edge()：三角形内部三个边函数同号，外部有异号");
    // 屏幕坐标 y 向下，这里的顶点顺序对应"绕序为负"
    const float area = edge(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
    CHECK(area < 0.0f);

    const float in0 = edge(0.0f, 0.0f, 1.0f, 0.0f, 0.25f, 0.25f);
    const float in1 = edge(1.0f, 0.0f, 0.0f, 1.0f, 0.25f, 0.25f);
    const float in2 = edge(0.0f, 1.0f, 0.0f, 0.0f, 0.25f, 0.25f);
    CHECK(in0 * area > 0.0f && in1 * area > 0.0f && in2 * area > 0.0f);

    const float out1 = edge(1.0f, 0.0f, 0.0f, 1.0f, 2.0f, 2.0f);  // 跨过斜边
    CHECK(out1 * area < 0.0f);
}

static void testToScreen() {
    TF_CASE("toScreen：ndc 原点映射到画面中心，ndc 右上角映射到 (w,0)");
    VSOut o;
    o.clip = {0.0f, 0.0f, 0.0f, 1.0f};
    SVert s = toScreen(o, 640, 480);
    CHECK_NEAR(s.x, 320.0f, 1e-4f);
    CHECK_NEAR(s.y, 240.0f, 1e-4f);
    CHECK_NEAR(s.invW, 1.0f, 1e-6f);

    o.clip = {1.0f, 1.0f, 0.0f, 1.0f};
    s = toScreen(o, 640, 480);
    CHECK_NEAR(s.x, 640.0f, 1e-4f);
    CHECK_NEAR(s.y, 0.0f, 1e-4f);  // y 向下

    o.clip = {0.0f, 0.0f, 0.0f, 4.0f};  // w 参与透视除法
    s = toScreen(o, 640, 480);
    CHECK_NEAR(s.invW, 0.25f, 1e-6f);
}

static void testDepthTest() {
    TF_CASE("drawTriangle：深度测试让近的三角形胜出，与绘制顺序无关");
    const Color red = rgb(255, 0, 0);
    const Color blue = rgb(0, 0, 255);
    const int total = 32 * 32;

    {   // 远的先画
        Framebuffer fb(32, 32);
        fb.clear(rgb(0, 0, 0));
        drawCover(fb, 0.5f, blue);
        drawCover(fb, -0.5f, red);
        CHECK(countColor(fb, red) == total);
        CHECK(countColor(fb, blue) == 0);
    }
    {   // 近的先画，远的不能覆盖
        Framebuffer fb(32, 32);
        fb.clear(rgb(0, 0, 0));
        drawCover(fb, -0.5f, red);
        drawCover(fb, 0.5f, blue);
        CHECK(countColor(fb, red) == total);
        CHECK(countColor(fb, blue) == 0);
    }
}

static void testBackfaceCull() {
    TF_CASE("drawTriangle：背面剔除按绕序丢弃（cullBack 打开时）");
    Framebuffer fb(32, 32);
    FlatShader shader;
    shader.color = rgb(0, 255, 0);

    // 与 testEdgeSign 同一个负面积三角形
    const SVert a = makeVert(0.0f, 0.0f, 0.0f);
    const SVert b = makeVert(31.0f, 0.0f, 0.0f);
    const SVert c = makeVert(0.0f, 31.0f, 0.0f);

    fb.clear(rgb(0, 0, 0));
    drawTriangle(fb, a, b, c, RasterOptions{true, true, true}, shader);
    CHECK(countColor(fb, rgb(0, 255, 0)) == 0);

    fb.clear(rgb(0, 0, 0));
    drawTriangle(fb, a, b, c, RasterOptions{false, true, true}, shader);
    CHECK(countColor(fb, rgb(0, 255, 0)) > 400);  // 约 31*31/2
}

static void testDegenerate() {
    TF_CASE("drawTriangle：退化三角形（三点共线 / 重复顶点）不写任何像素");
    Framebuffer fb(16, 16);
    FlatShader shader;
    shader.color = rgb(10, 20, 30);
    fb.clear(rgb(0, 0, 0));

    const RasterOptions opt{false, true, true};
    drawTriangle(fb, makeVert(2.0f, 2.0f, 0.0f), makeVert(8.0f, 2.0f, 0.0f),
                 makeVert(14.0f, 2.0f, 0.0f), opt, shader);
    drawTriangle(fb, makeVert(4.0f, 4.0f, 0.0f), makeVert(4.0f, 4.0f, 0.0f),
                 makeVert(4.0f, 4.0f, 0.0f), opt, shader);

    CHECK(countColor(fb, rgb(10, 20, 30)) == 0);
}

void testRaster() {
    testEdgeSign();
    testToScreen();
    testDepthTest();
    testBackfaceCull();
    testDegenerate();
}
