#pragma once

#include "core/framebuffer.h"  // Color
#include "math/vec.h"          // Vec2
#include "pipeline/vertex.h"   // VSOut / Fragment

// 光栅化器内部的屏幕空间顶点（指导 §5.2 的 SVert）。
// 属性一律存两份：原始值（仿射插值用）和除以 w 的值（透视校正插值用）。
struct SVert {
    float x = 0, y = 0;   // 像素坐标，左上原点，y 向下
    float z = 0;          // NDC z ∈ [-1,1]
    float invW = 1;       // 1/w

    Vec3 worldPos;
    Vec3 worldPosOverW;
    Vec3 normal;
    Vec3 normalOverW;
    Vec2 uv;
    Vec2 uvOverW;
};

// 裁剪空间 → 屏幕空间：透视除法 + 视口变换（指导 §7.4）
SVert toScreen(const VSOut& v, int width, int height);

// Bresenham 画线；端点先四舍五入，画前 clamp 到屏幕内
void drawLine(Framebuffer& fb, const SVert& a, const SVert& b, Color color);

// (p-a)×(b-a) 的 z 分量：p 在 a→b 的左侧为正
inline float edge(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

// 片元着色器接口（指导 §5.3）。D3 先用虚函数，清楚；性能阶段再换模板。
class IShader {
public:
    virtual ~IShader() = default;
    virtual Color fragment(const Fragment& f) const = 0;
};

// 边函数光栅化 + 深度测试 + 背面剔除（指导 §7.2 + §7.5）
void drawTriangle(Framebuffer& fb, SVert v0, SVert v1, SVert v2,
                  bool cullBack, bool perspectiveCorrect, const IShader& shader);
