#pragma once

#include <vector>

#include "core/framebuffer.h"  // Color
#include "math/vec.h"          // Vec2

// uv 超出 [0,1] 时怎么取：Clamp 用边缘纹素，Repeat 平铺（对应 GL_CLAMP/GL_REPEAT）
enum class Wrap { Clamp, Repeat };

// 2D 纹理：一块 Color 纹素 + 双线性采样
class Texture {
public:
    Texture(int w, int h);

    int width() const { return m_width; }
    int height() const { return m_height; }

    Color texel(int x, int y) const;   // 边界 clamp 后取值
    void setTexel(int x, int y, Color c);
    Color sample(Vec2 uv) const;       // 双线性采样

    void setWrap(Wrap w) { m_wrap = w; }
    Wrap wrap() const { return m_wrap; }

private:
    int wrapIndex(int i, int n) const;

    int m_width = 0;
    int m_height = 0;
    std::vector<Color> m_texels;
    Wrap m_wrap = Wrap::Clamp;
};
