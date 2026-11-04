#include "core/framebuffer.h"

#include <algorithm>
#include <fstream>

Framebuffer::Framebuffer(int width, int height) : m_width(width), m_height(height) {
    if (width <= 0 || height <= 0) {
        m_width = 0;
        m_height = 0;
        return;
    }
    m_color.assign(size_t(width) * size_t(height), 0xFF000000u);
    m_depth.assign(size_t(width) * size_t(height), 1.0f);
}

void Framebuffer::clear(Color c, float depth) {
    std::fill(m_color.begin(), m_color.end(), c);
    std::fill(m_depth.begin(), m_depth.end(), depth);
}

void Framebuffer::setPixel(int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= m_width || y >= m_height) return;
    m_color[size_t(y) * size_t(m_width) + size_t(x)] = c;
}

bool Framebuffer::exportTGA(const std::string& path) const {
    if (m_width <= 0 || m_height <= 0) return false;

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    // TGA 头 18 字节
    uint8_t header[18] = {};
    header[2] = 2;  // 数据类型：未压缩真彩色
    header[12] = uint8_t(m_width & 0xFF);
    header[13] = uint8_t((m_width >> 8) & 0xFF);
    header[14] = uint8_t(m_height & 0xFF);
    header[15] = uint8_t((m_height >> 8) & 0xFF);
    header[16] = 24;    // 每像素 24 位（BGR）
    header[17] = 0x20;  // 原点在左上角（bit 5），alpha 位数 0

    out.write(reinterpret_cast<const char*>(header), sizeof(header));

    std::vector<uint8_t> row(size_t(m_width) * 3);
    for (int y = 0; y < m_height; ++y) {
        const Color* src = m_color.data() + size_t(y) * size_t(m_width);
        for (int x = 0; x < m_width; ++x) {
            const Color c = src[x];
            row[size_t(x) * 3 + 0] = uint8_t(c & 0xFFu);          // B
            row[size_t(x) * 3 + 1] = uint8_t((c >> 8) & 0xFFu);   // G
            row[size_t(x) * 3 + 2] = uint8_t((c >> 16) & 0xFFu);  // R
        }
        out.write(reinterpret_cast<const char*>(row.data()), std::streamsize(row.size()));
    }

    return out.good();
}
