#pragma once

#include <algorithm>
#include <cmath>

#include "math/mat.h"
#include "math/vec.h"

// 轨道相机：眼位永远落在以 target 为球心的球面上，用三个数描述：
//   yaw      绕 Y 轴转的角度（0 时相机在 +Z 方向）
//   pitch    仰角，0 表示与 target 齐平，+π/2 表示正上方
//   distance 球面半径，也就是"离物体多远"
// 鼠标拖动改 yaw/pitch、滚轮改 distance，就是所有 3D 查看器的标准手感。
struct OrbitCamera {
    Vec3 target{0.0f, 0.0f, 0.0f};
    float yaw = 0.55f;
    float pitch = 0.35f;
    float distance = 6.0f;

    Vec3 eye() const {
        // distance 为 0 时 eye 与 target 重合，lookAt 的前向向量退化成 0 → 全屏 NaN。
        // 卡一个极小值：这样 --dist 0 也只是"贴到物体表面"，不会炸。
        const float d = std::max(distance, 1e-3f);
        const float cp = std::cos(pitch);
        const Vec3 dir{std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp};
        return target + dir * d;
    }

    Mat4 view() const { return lookAt(eye(), target, Vec3{0.0f, 1.0f, 0.0f}); }

    // 像素位移 → 角度：0.007 rad/px 大约等于"拖过整屏转半圈"的手感
    void orbit(float dxPixels, float dyPixels) {
        yaw -= dxPixels * 0.007f;
        // pitch 卡在 ±1.5（≈86°）：避免到 90° 时 lookAt 的 up 与视线共线、叉积退化
        pitch = std::clamp(pitch + dyPixels * 0.007f, -1.5f, 1.5f);
    }

    // 滚轮一格缩放 12%，比线性加减在任何距离下都好用
    void zoom(float wheelSteps) {
        distance = std::clamp(distance * std::pow(0.88f, wheelSteps), 1.5f, 40.0f);
    }

    void reset() {
        yaw = 0.55f;
        pitch = 0.35f;
        distance = 6.0f;
    }
};
