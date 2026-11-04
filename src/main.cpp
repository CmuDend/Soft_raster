#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>

#include "core/framebuffer.h"
#include "platform/window_sdl.h"

namespace {

constexpr int kWidth = 960;
constexpr int kHeight = 540;

// D0 唯一的事：往 framebuffer 里写像素，证明"内存 → 窗口"这条链路是通的。
// 真正的几何管线从 D1 开始，那时候像素的来源会换成 rasterizer。
void drawGradient(Framebuffer& fb) {
    for (int y = 0; y < fb.height(); ++y) {
        const uint8_t g = uint8_t(255 * y / (fb.height() - 1));
        for (int x = 0; x < fb.width(); ++x) {
            const uint8_t r = uint8_t(255 * x / (fb.width() - 1));
            fb.setPixel(x, y, rgb(r, g, 128));
        }
    }
}

}  // namespace

int main() {
    try {
        Framebuffer fb(kWidth, kHeight);
        drawGradient(fb);

        // 验收物之一：内存里的像素能落成文件，不开窗也能验证画对了没有
        if (fb.exportTGA("out.tga")) {
            std::puts("已导出 out.tga");
        } else {
            std::fprintf(stderr, "导出 out.tga 失败\n");
        }

        Window window("soft raster | D0", kWidth, kHeight);
        std::puts("按 S 重新导出 out.tga，ESC 或关窗退出。");

        bool running = true;
        int frames = 0;
        auto fpsTimer = std::chrono::steady_clock::now();

        while (running) {
            const FrameInput in = window.pollEvents();
            if (in.quit) running = false;
            if (in.saveRequested && fb.exportTGA("out.tga")) std::puts("已导出 out.tga");

            window.present(fb);

            ++frames;
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - fpsTimer).count();
            if (elapsed >= 0.5) {
                char title[64];
                std::snprintf(title, sizeof(title), "soft raster | D0 | %.0f FPS", frames / elapsed);
                window.setTitle(title);
                frames = 0;
                fpsTimer = now;
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "错误: %s\n", e.what());
        return 1;
    }
    return 0;
}
