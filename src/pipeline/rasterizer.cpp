#include "pipeline/rasterizer.h"

#include <algorithm>
#include <cmath>

SVert toScreen(const Vec4& clip, int width, int height) {
    float invW = 1.0f / clip.w;
    float ndcX = clip.x * invW;
    float ndcY = clip.y * invW;
    float ndcZ = clip.z * invW;
    
    // TODO：透视除法 + 视口变换（指导 §7.4）
    // 1) invW = 1/clip.w
    // 2) ndcX = clip.x*invW, ndcY = clip.y*invW, ndcZ = clip.z*invW
    // 3) s.x = (ndcX*0.5+0.5)*width ; s.y = (1 - (ndcY*0.5+0.5))*height ; s.z = ndcZ ; s.invW = invW
    SVert s;
    s.x = (ndcX * 0.5 + 0.5) * width;
    s.y = (1.0f - (ndcY * 0.5f + 0.5f)) * float(height);
    s.z = ndcZ;
    s.invW = invW;
    return s;
}

void drawLine(Framebuffer& fb, const SVert& a, const SVert& b, Color color) {
    // TODO：Bresenham 画线
    // 1) int x0 = (int)std::lround(a.x); 同理 y0、x1、y1
    // 2) 标准 Bresenham 循环（任意斜率都对的版本）
    // 3) 写像素前判断 x 在 [0,width-1]、y 在 [0,height-1]，越界就跳过
    int x0 = (int)std::lround(a.x);
    int y0 = (int)std::lround(a.y);
    int x1 = (int)std::lround(b.x);
    int y1 = (int)std::lround(b.y);

    int dx = std::abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true){
        if (x0 >= 0 && x0 < fb.width() && y0 >=0 && y0 < fb.height())
            fb.setPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }

    (void)fb; (void)a; (void)b; (void)color;
}

// ===== D2：三角形光栅化 =====
void drawTriangle(Framebuffer& fb, SVert v0, SVert v1, SVert v2, Color color, bool cullBack) {
    // 1) 面积（edge 你自己先填，否则这里恒为 0）
    float area = edge(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y);
    if (std::fabs(area) < 1e-8f) return;  // 2) 退化三角形，避免除 0

    // 3) 背面剔除（正面 = area > 0，D2 任务书第 4 节已验证）
    if (cullBack && area < 0.0f) return;
    // 4) 统一绕序成 area > 0，后面三个边函数才都 >= 0
    if (area < 0.0f) { std::swap(v1, v2); area = -area; }

    const float invArea = 1.0f / area;

    // 5) 包围盒：floor(min) / ceil(max)，再 clamp 到屏幕（越界写内存是新手第一个段错误）
    const int minX = std::max(0, static_cast<int>(std::floor(std::min({v0.x, v1.x, v2.x}))));
    const int maxX = std::min(fb.width() - 1, static_cast<int>(std::ceil(std::max({v0.x, v1.x, v2.x}))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({v0.y, v1.y, v2.y}))));
    const int maxY = std::min(fb.height() - 1, static_cast<int>(std::ceil(std::max({v0.y, v1.y, v2.y}))));
    if (minX > maxX || minY > maxY) return;

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            // TODO 6) 采样点取像素中心：px = x + 0.5f, py = y + 0.5f
            const float px = x + 0.5f, py = y + 0.5f;
            // TODO 7) 重心坐标（用 edge 和 invArea）：
            const float w0 = edge(v1.x, v1.y, v2.x, v2.y, px ,py) * invArea;
            const float w1 = edge(v2.x, v2.y, v0.x, v0.y, px ,py) * invArea;
            const float w2 = 1.0f - w0 - w1;  // 省一次边函数
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;  // 在三角形外面

            const float z01 = (w0 * v0.z + w1 * v1.z + w2 * v2.z) * 0.5f + 0.5f;  // TODO 8) 深度
            const size_t idx = static_cast<size_t>(y) * fb.width() + static_cast<size_t>(x);
            if (z01 >= fb.depthData()[idx]) continue;
            fb.depthData()[idx] = z01;
            fb.colorData()[idx] = color;  // TODO 9) 深度测试
            
        }
    }

   
}
