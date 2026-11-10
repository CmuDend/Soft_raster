#pragma once

#include "core/framebuffer.h"  // Color
#include "math/vec.h"          // Vec4

// 光栅化器内部的屏幕空间顶点（指导 §5.2 的 SVert，D1 只用前三个字段）
struct SVert {
    float x = 0, y = 0;   // 像素坐标，左上原点，y 向下
    float z = 0;          // NDC z ∈ [-1,1]
    float invW = 1;       // D2 插值才用；D1 先算出来放着
};

// 裁剪空间 → 屏幕空间：透视除法 + 视口变换（指导 §7.4）
SVert toScreen(const Vec4& clip, int width, int height);

// Bresenham 画线；端点先四舍五入，画前 clamp 到屏幕内
void drawLine(Framebuffer& fb, const SVert& a, const SVert& b, Color color);
