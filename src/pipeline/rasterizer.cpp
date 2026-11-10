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
