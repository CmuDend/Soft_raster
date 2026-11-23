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

// 光栅化的开关。拆成结构体是为了 bench 能一次只改一个变量（D6）。
// scissor 是 D7 加的：tile 渲染时每个 tile 只许写自己那块像素，写集合互不相交，
// 于是多线程下既不需要锁，也不会有假共享（相邻 tile 至少隔 32 px = 128 B）。
struct RasterOptions {
    bool cullBack = true;            // 背面剔除
    bool perspectiveCorrect = true;  // 透视校正插值
    bool incrementalEdges = true;    // 增量边函数；false = D2 的逐像素重算，做对照

    int x0 = 0;                      // scissor 左上（含）
    int y0 = 0;
    int x1 = 0x7fffffff;             // scissor 右下（不含）
    int y1 = 0x7fffffff;
};

// 片元计数器：overdraw = covered / shaded，bench 用它说话
struct RasterCounters {
    unsigned long long covered = 0;  // 通过覆盖测试的片元
    unsigned long long shaded = 0;   // 通过深度测试、真正着色了的片元
};

// 边函数光栅化 + 深度测试 + 背面剔除（指导 §7.2 + §7.5）
void drawTriangle(Framebuffer& fb, SVert v0, SVert v1, SVert v2, const RasterOptions& opt,
                  const IShader& shader, RasterCounters* counters = nullptr);
