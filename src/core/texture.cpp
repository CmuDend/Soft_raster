#include "core/texture.h"

#include <algorithm>
#include <cmath>

namespace {

// 两个打包颜色按 t 线性插值：拆成 R/G/B 三通道各自插，再拼回去
Color lerpColor(Color a, Color b, float t) {
    auto ch = [&](int shift) -> uint32_t {
        const float fa = float((a >> shift) & 0xFFu);
        const float fb = float((b >> shift) & 0xFFu);
        return uint32_t(fa + (fb - fa) * t + 0.5f);  // +0.5 是四舍五入
    };
    return 0xFF000000u | (ch(16) << 16) | (ch(8) << 8) | ch(0);
}

}  // namespace

Texture::Texture(int w, int h) : m_width(w), m_height(h), m_texels(size_t(w) * size_t(h)) {}

// 取值前把坐标 clamp 到边界内，这样双线性取 x0+1 越界也不会崩
Color Texture::texel(int x, int y) const {
    x = std::clamp(x, 0, m_width - 1);
    y = std::clamp(y, 0, m_height - 1);
    return m_texels[size_t(y) * size_t(m_width) + size_t(x)];
}

void Texture::setTexel(int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= m_width || y >= m_height) return;
    m_texels[size_t(y) * size_t(m_width) + size_t(x)] = c;
}

Color Texture::sample(Vec2 uv) const {
    // 1) uv ∈ [0,1] -> 连续纹素坐标；减 0.5 让整数坐标对齐纹素中心
    const float u = uv.x * float(m_width) - 0.5f;
    const float v = uv.y * float(m_height) - 0.5f;

    // 2) 左/下邻纹素编号 + 小数部分（距左邻中心的距离，∈ [0,1)）
    const int x0 = int(std::floor(u));
    const int y0 = int(std::floor(v));
    const float fx = u - float(x0);
    const float fy = v - float(y0);

    // 3) 四个邻居（texel 内部 clamp，x0=-1 或 x0+1=width 都安全）
    const Color c00 = texel(x0, y0);
    const Color c10 = texel(x0 + 1, y0);
    const Color c01 = texel(x0, y0 + 1);
    const Color c11 = texel(x0 + 1, y0 + 1);

    // 4) 先水平两次，再垂直一次
    const Color bottom = lerpColor(c00, c10, fx);
    const Color top = lerpColor(c01, c11, fx);
    return lerpColor(bottom, top, fy);
}
