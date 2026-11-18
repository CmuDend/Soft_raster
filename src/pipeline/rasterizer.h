#pragma once

#include "core/framebuffer.h"  // Color
#include "math/vec.h"          // Vec4

// 光栅化器内部的屏幕空间顶点（指导 §5.2 的 SVert）
struct SVert {
    float x = 0, y = 0;   // 像素坐标，左上原点，y 向下
    float z = 0;          // NDC z ∈ [-1,1]
    float invW = 1;       // D3 插值才用；D2 先放着
    Vec2 uv;
    Vec2 uvOverW;

};

// 裁剪空间 → 屏幕空间：透视除法 + 视口变换（指导 §7.4）
SVert toScreen(const Vec4& clip, const Vec2& uv, int width, int height);

// Bresenham 画线；端点先四舍五入，画前 clamp 到屏幕内
void drawLine(Framebuffer& fb, const SVert& a, const SVert& b, Color color);

// ===== D2 新增 =====

// (p-a)×(b-a) 的 z 分量：p 在 a→b 的左侧为正
inline float edge(float ax, float ay, float bx, float by, float px, float py) {
    // TODO：一行（公式在 D2 任务书第 2 节）
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

// 边函数光栅化 + 深度测试 + 背面剔除（指导 §7.2 + §7.5）
void drawTriangle(Framebuffer& fb, SVert v0, SVert v1, SVert v2, bool cullBack, bool perspectiveCorrect, Color (*shade)(Vec2));
