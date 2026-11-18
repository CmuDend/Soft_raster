#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "core/framebuffer.h"
#include "math/mat.h"
#include "math/vec.h"
#include "pipeline/rasterizer.h"
#include "pipeline/vertex.h"
#include "platform/window_sdl.h"

namespace {

constexpr int kWidth = 960;
constexpr int kHeight = 540;
constexpr float kPi = 3.14159265358979323846f;

// D2 验证顺序：先 kCullBack = false 测深度（应看到实心立方体），
// 深度对了再改成 true 测剔除（应还是实心，但画得少一半）。
constexpr bool kCullBack = true;

Color shadeChecker(Vec2 uv) {
    const int N = 8;
    int cx = (int)std::floor(uv.x * N);
    int cy = (int)std::floor(uv.y * N);
    return ((cx + cy) & 1) ? rgb(20, 20, 20) : rgb(220, 220, 220);
}

constexpr bool kPerspectiveCorrect = true;   // 先 false 看扭曲

void drawScene(Framebuffer& fb) {
    fb.clear(rgb(16, 16, 24));

    const Mat4 view = lookAt({0, 0.6f, 3.0f}, {0, -0.6f, -1.0f}, {0, 1, 0});
    const Mat4 proj = perspective(60.0f * kPi / 180.0f, float(kWidth) / float(kHeight), 0.1f, 100.0f);
    const Mat4 mvp = proj * view;   // 地面不旋转，model 是单位阵

    const Vertex quad[4] = {
        {{-2, -1, -2}, {0, 0}},
        {{ 2, -1, -2}, {1, 0}},
        {{ 2, -1,  2}, {1, 1}},
        {{-2, -1,  2}, {0, 1}},
    };
    const int tris[2][3] = {{0, 2, 1}, {0, 3, 2}};   // +Y 朝上（相机在上方往下看）

    for (const auto& t : tris) {
        SVert s[3];
        bool ok = true;
        for (int i = 0; i < 3; ++i) {
            const Vertex& v = quad[t[i]];
            const Vec4 clip = mvp * Vec4{v.position.x, v.position.y, v.position.z, 1.0f};
            if (clip.w <= 1e-5f) { ok = false; break; }
            s[i] = toScreen(clip, v.uv, kWidth, kHeight);
        }
        if (ok) drawTriangle(fb, s[0], s[1], s[2], kCullBack, kPerspectiveCorrect, shadeChecker);
    }
}

}  // namespace

int main() {
    try {
        Framebuffer fb(kWidth, kHeight);
        drawScene(fb);

        if (fb.exportTGA("out.tga")) {
            std::puts("已导出 out.tga");
        } else {
            std::fprintf(stderr, "导出 out.tga 失败\n");
        }

        Window window("soft raster | D2", kWidth, kHeight);
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
                std::snprintf(title, sizeof(title), "soft raster | D2 | %.0f FPS", frames / elapsed);
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
