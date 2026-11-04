#pragma once

#include <cstdint>
#include <string>
#include <vector>

// 像素内存布局：0xAARRGGBB。
// 与 SDL_PIXELFORMAT_ARGB8888 的 32 位打包值一致，所以上屏时可以整块上传，
// 不需要逐像素做格式转换 —— 这就是指导 §9 里 blit 链路的全部。
using Color = uint32_t;

inline Color rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

// 颜色缓冲 + 深度缓冲。
// D0 只用到颜色；深度数组现在就分配好，D2 才往里面写。
class Framebuffer {
public:
    Framebuffer(int width, int height);

    int width() const { return m_width; }
    int height() const { return m_height; }

    Color* colorData() { return m_color.data(); }
    const Color* colorData() const { return m_color.data(); }
    float* depthData() { return m_depth.data(); }

    void clear(Color c, float depth = 1.0f);

    // 带边界检查的写像素；D0 用，D2 光栅化热路径会换成直接按下标写。
    void setPixel(int x, int y, Color c);

    // 24 位未压缩 TGA（原点左上，BGR 顺序）。格式简单、无依赖，方便 diff。
    bool exportTGA(const std::string& path) const;

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<Color> m_color;
    std::vector<float> m_depth;
};
