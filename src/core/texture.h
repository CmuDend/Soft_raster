#pragma once

#include <vector>

#include "core/framebuffer.h"  // Color
#include "math/vec.h"          // Vec2

// 2D 纹理：一块 Color 纹素 + 双线性采样
class Texture {
public:
    Texture(int w, int h);

    int width() const { return m_width; }
    int height() const { return m_height; }

    Color texel(int x, int y) const;   // 边界 clamp 后取值
    void setTexel(int x, int y, Color c);
    Color sample(Vec2 uv) const;       // 双线性采样

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<Color> m_texels;
};
