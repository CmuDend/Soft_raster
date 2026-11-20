#include "pipeline/rasterizer.h"

#include <algorithm>
#include <cmath>

SVert toScreen(const VSOut& v, int width, int height) {
    const float invW = 1.0f / v.clip.w;
    const float ndcX = v.clip.x * invW;
    const float ndcY = v.clip.y * invW;
    const float ndcZ = v.clip.z * invW;

    SVert s;
    s.x = (ndcX * 0.5f + 0.5f) * float(width);
    s.y = (1.0f - (ndcY * 0.5f + 0.5f)) * float(height);  // y 向下
    s.z = ndcZ;                                            // 保持 [-1,1]
    s.invW = invW;

    // 属性存两份：原值 + 除以 w 的值，光栅化时二选一（透视校正开关）
    s.worldPos = v.worldPos;
    s.worldPosOverW = v.worldPos * invW;
    s.normal = v.normal;
    s.normalOverW = v.normal * invW;
    s.uv = v.uv;
    s.uvOverW = v.uv * invW;
    return s;
}

void drawLine(Framebuffer& fb, const SVert& a, const SVert& b, Color color) {
    int x0 = (int)std::lround(a.x);
    int y0 = (int)std::lround(a.y);
    int x1 = (int)std::lround(b.x);
    int y1 = (int)std::lround(b.y);

    int dx = std::abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        if (x0 >= 0 && x0 < fb.width() && y0 >= 0 && y0 < fb.height())
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
}

// ===== D2：三角形光栅化（D3 起改为调用片元着色器）=====
void drawTriangle(Framebuffer& fb, SVert v0, SVert v1, SVert v2,
                  bool cullBack, bool perspectiveCorrect, const IShader& shader) {
    float area = edge(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y);
    if (std::fabs(area) < 1e-8f) return;          // 退化三角形，避免除 0

    if (cullBack && area < 0.0f) return;          // 正面 = area > 0
    if (area < 0.0f) { std::swap(v1, v2); area = -area; }

    const float invArea = 1.0f / area;

    const int minX = std::max(0, static_cast<int>(std::floor(std::min({v0.x, v1.x, v2.x}))));
    const int maxX = std::min(fb.width() - 1, static_cast<int>(std::ceil(std::max({v0.x, v1.x, v2.x}))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({v0.y, v1.y, v2.y}))));
    const int maxY = std::min(fb.height() - 1, static_cast<int>(std::ceil(std::max({v0.y, v1.y, v2.y}))));
    if (minX > maxX || minY > maxY) return;

    Color* color = fb.colorData();
    float* depth = fb.depthData();

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float px = x + 0.5f, py = y + 0.5f;   // 像素中心

            const float w0 = edge(v1.x, v1.y, v2.x, v2.y, px, py) * invArea;
            const float w1 = edge(v2.x, v2.y, v0.x, v0.y, px, py) * invArea;
            const float w2 = 1.0f - w0 - w1;            // 省一次边函数
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;

            // 深度：z_ndc 在屏幕空间是仿射的，直接线性插值就精确（指导 §7.6）
            const float z01 = (w0 * v0.z + w1 * v1.z + w2 * v2.z) * 0.5f + 0.5f;
            const size_t idx = size_t(y) * size_t(fb.width()) + size_t(x);
            if (z01 >= depth[idx]) continue;            // 深度测试（early-Z）

            Fragment f;
            if (perspectiveCorrect) {
                const float invW = w0 * v0.invW + w1 * v1.invW + w2 * v2.invW;
                if (invW <= 0.0f) continue;
                const float r = 1.0f / invW;            // 插值 α/w 再除以插值的 1/w
                f.uv = (v0.uvOverW * w0 + v1.uvOverW * w1 + v2.uvOverW * w2) * r;
                f.normal = (v0.normalOverW * w0 + v1.normalOverW * w1 + v2.normalOverW * w2) * r;
                f.worldPos = (v0.worldPosOverW * w0 + v1.worldPosOverW * w1 + v2.worldPosOverW * w2) * r;
            } else {
                f.uv = v0.uv * w0 + v1.uv * w1 + v2.uv * w2;
                f.normal = v0.normal * w0 + v1.normal * w1 + v2.normal * w2;
                f.worldPos = v0.worldPos * w0 + v1.worldPos * w1 + v2.worldPos * w2;
            }

            depth[idx] = z01;
            color[idx] = shader.fragment(f);
        }
    }
}
