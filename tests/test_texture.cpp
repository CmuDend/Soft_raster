#include "test_framework.h"

#include "core/texture.h"

namespace {
int redOf(Color c) { return int((c >> 16) & 0xFF); }
int greenOf(Color c) { return int((c >> 8) & 0xFF); }
int blueOf(Color c) { return int(c & 0xFF); }
}  // namespace

static void testTexelCenter() {
    TF_CASE("sample：uv 落在纹素中心时返回该纹素的原始颜色");
    Texture t(2, 2);
    t.setTexel(0, 0, rgb(10, 20, 30));
    t.setTexel(1, 0, rgb(200, 100, 50));
    t.setTexel(0, 1, rgb(60, 70, 80));
    t.setTexel(1, 1, rgb(90, 91, 92));

    // 纹素 (0,0) 的中心：u = (0+0.5)/2 = 0.25
    const Color c = t.sample({0.25f, 0.25f});
    CHECK(redOf(c) == 10 && greenOf(c) == 20 && blueOf(c) == 30);

    const Color d = t.sample({0.75f, 0.75f});
    CHECK(redOf(d) == 90 && greenOf(d) == 91 && blueOf(d) == 92);
}

static void testBilinearMidpoint() {
    TF_CASE("sample：两个纹素正中 → 各通道取平均值");
    Texture t(2, 2);
    t.setTexel(0, 0, rgb(0, 0, 0));
    t.setTexel(1, 0, rgb(255, 255, 255));
    t.setTexel(0, 1, rgb(0, 0, 0));
    t.setTexel(1, 1, rgb(255, 255, 255));

    const Color c = t.sample({0.5f, 0.25f});  // u = 0.5 → 横向各半
    CHECK_NEAR(redOf(c), 127.5, 1.0);
    CHECK_NEAR(greenOf(c), 127.5, 1.0);
    CHECK_NEAR(blueOf(c), 127.5, 1.0);
}

static void testWrapRepeat() {
    TF_CASE("sample：Wrap::Repeat 下 uv 平移整数倍仍然采到同一纹素");
    Texture t(4, 1);
    for (int x = 0; x < 4; ++x) t.setTexel(x, 0, rgb(uint8_t(x * 60), 0, 0));
    t.setWrap(Wrap::Repeat);

    const Vec2 uv{0.3f, 0.5f};
    const Color a = t.sample(uv);
    const Color b = t.sample({uv.x + 1.0f, uv.y});
    const Color c = t.sample({uv.x - 2.0f, uv.y});
    CHECK(a == b);
    CHECK(a == c);
}

static void testWrapClamp() {
    TF_CASE("sample：Wrap::Clamp 下 uv 越界退化成边缘纹素");
    Texture t(4, 1);
    for (int x = 0; x < 4; ++x) t.setTexel(x, 0, rgb(uint8_t(x * 60), 0, 0));
    t.setWrap(Wrap::Clamp);

    const Color edge = t.sample({0.875f, 0.5f});  // 最后一个纹素的中心
    const Color beyond = t.sample({5.0f, 0.5f});
    CHECK(edge == beyond);
}

void testTexture() {
    testTexelCenter();
    testBilinearMidpoint();
    testWrapRepeat();
    testWrapClamp();
}
