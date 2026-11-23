#include "test_framework.h"

#include "math/mat.h"
#include "math/vec.h"

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

static void testPerspectiveDepth() {
    TF_CASE("perspective：eye z=-near → ndc z=-1，eye z=-far → ndc z=+1");
    const float n = 0.1f, f = 100.0f;
    const Mat4 p = perspective(kPi * 0.5f, 1.0f, n, f);

    const auto ndcZ = [&](float eyeZ) {
        const Vec4 c = p * Vec4{0.0f, 0.0f, eyeZ, 1.0f};
        return c.z / c.w;
    };

    CHECK_NEAR(ndcZ(-n), -1.0f, 1e-4f);
    CHECK_NEAR(ndcZ(-f), 1.0f, 1e-3f);
    CHECK_NEAR((p * Vec4{0.0f, 0.0f, -3.0f, 1.0f}).w, 3.0f, 1e-5f);  // w = -z_eye

    // 深度是非线性的，但必须单调递增
    CHECK(ndcZ(-2.0f) < ndcZ(-10.0f));
    CHECK(ndcZ(-10.0f) < ndcZ(-50.0f));
}

static void testLookAt() {
    TF_CASE("lookAt：相机看向原点时，原点落在视图空间 (0,0,-距离)");
    const Mat4 v = lookAt({0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});

    const Vec4 origin = v * Vec4{0.0f, 0.0f, 0.0f, 1.0f};
    CHECK_NEAR(origin.x, 0.0f, 1e-5f);
    CHECK_NEAR(origin.y, 0.0f, 1e-5f);
    CHECK_NEAR(origin.z, -5.0f, 1e-5f);

    // 世界 +x 仍是相机右方，世界 +y 仍是相机上方
    const Vec4 right = v * Vec4{1.0f, 0.0f, 5.0f, 1.0f};
    CHECK_NEAR(right.x, 1.0f, 1e-5f);
    CHECK_NEAR(right.z, 0.0f, 1e-5f);
}

static void testMatMultiply() {
    TF_CASE("Mat4 乘法：单位阵、平移叠加、rotationY 方向");
    const Mat4 m = translation({1.0f, 2.0f, 3.0f}) * rotationY(0.3f);
    const Mat4 m2 = m * Mat4::identity();
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) CHECK_NEAR(m2(r, c), m(r, c), 1e-6f);

    const Vec4 t = translation({1.0f, 2.0f, 3.0f}) * translation({4.0f, 5.0f, 6.0f}) *
                   Vec4{0.0f, 0.0f, 0.0f, 1.0f};
    CHECK_NEAR(t.x, 5.0f, 1e-5f);
    CHECK_NEAR(t.y, 7.0f, 1e-5f);
    CHECK_NEAR(t.z, 9.0f, 1e-5f);

    // 右手系绕 +y 转 90°：(1,0,0) → (0,0,-1)
    const Vec4 r = rotationY(kPi * 0.5f) * Vec4{1.0f, 0.0f, 0.0f, 1.0f};
    CHECK_NEAR(r.x, 0.0f, 1e-5f);
    CHECK_NEAR(r.z, -1.0f, 1e-5f);
}

static void testNormalMatrix() {
    TF_CASE("法线矩阵：非等比缩放下法线仍垂直于切平面");
    const Mat4 model =
        translation({1.0f, 2.0f, 3.0f}) * rotationY(0.6f) * scaling({2.0f, 1.0f, 0.5f});

    const Mat3 m3 = upper3x3(model);
    const Mat3 nm = normalMatrix(model);

    const Vec3 n{0.70710678f, 0.0f, 0.70710678f};   // 切平面法线
    const Vec3 t{0.70710678f, 0.0f, -0.70710678f};  // 切平面内的方向

    const Vec3 nPrime = normalize(nm * n);
    const Vec3 tPrime = normalize(m3 * t);
    CHECK_NEAR(dot(nPrime, tPrime), 0.0f, 1e-4f);

    // 反例：直接拿 M 变换法线，正交性就破坏了
    const Vec3 wrong = normalize(m3 * n);
    CHECK(std::fabs(dot(wrong, tPrime)) > 0.3f);
}

static void testVecBasics() {
    TF_CASE("Vec3 基础：cross / dot / length / normalize");
    const Vec3 z = cross({1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    CHECK_NEAR(z.x, 0.0f, 1e-6f);
    CHECK_NEAR(z.y, 0.0f, 1e-6f);
    CHECK_NEAR(z.z, 1.0f, 1e-6f);

    CHECK_NEAR(dot({1.0f, 2.0f, 3.0f}, {4.0f, -5.0f, 6.0f}), 12.0f, 1e-6f);
    CHECK_NEAR(length({3.0f, 4.0f, 0.0f}), 5.0f, 1e-6f);

    const Vec3 n = normalize({0.0f, 0.0f, 7.0f});
    CHECK_NEAR(n.z, 1.0f, 1e-6f);

    const Vec3 zero = normalize({0.0f, 0.0f, 0.0f});  // 不能出 NaN
    CHECK_NEAR(zero.x, 0.0f, 1e-6f);
}

void testMath() {
    testPerspectiveDepth();
    testLookAt();
    testMatMultiply();
    testNormalMatrix();
    testVecBasics();
}
