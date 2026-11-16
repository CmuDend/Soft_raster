#include <chrono>
#include <cstdint>
#include <cstdio>

#include "core/framebuffer.h"
#include "math/mat.h"
#include "math/vec.h"
#include "pipeline/rasterizer.h"
#include "platform/window_sdl.h"

namespace {

constexpr int kWidth = 960;
constexpr int kHeight = 540;
constexpr float kPi = 3.14159265358979323846f;

// D2 验证顺序：先 kCullBack = false 测深度（应看到实心立方体），
// 深度对了再改成 true 测剔除（应还是实心，但画得少一半）。
constexpr bool kCullBack = true;

// 立方体 8 个顶点
const Vec3 kCube[8] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},
};

// 12 个三角形（CCW 朝外，已算好绕序），每个面一个颜色
struct Tri {
    int idx[3];
    Color color;
};
const Tri kTris[12] = {
    {{4, 5, 6}, rgb(90, 190, 110)},  {{4, 6, 7}, rgb(90, 190, 110)},   // +Z 绿
    {{0, 3, 2}, rgb(170, 90, 190)},  {{0, 2, 1}, rgb(170, 90, 190)},   // -Z 紫
    {{1, 2, 6}, rgb(200, 70, 60)},   {{1, 6, 5}, rgb(200, 70, 60)},    // +X 红
    {{0, 7, 3}, rgb(60, 90, 200)},   {{0, 4, 7}, rgb(60, 90, 200)},    // -X 蓝
    {{3, 6, 2}, rgb(210, 200, 90)},  {{3, 7, 6}, rgb(210, 200, 90)},   // +Y 黄
    {{0, 1, 5}, rgb(120, 120, 130)}, {{0, 5, 4}, rgb(120, 120, 130)},  // -Y 灰
};

// 实心立方体：M(model) 第一次登场，把立方体转个角度让 3 个面露出来
void drawCube(Framebuffer& fb) {
    fb.clear(rgb(16, 16, 24));

    const Mat4 view = lookAt({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    const Mat4 proj = perspective(60.0f * kPi / 180.0f, float(kWidth) / float(kHeight), 0.1f, 100.0f);
    const Mat4 model = rotationY(0.7f) * rotationX(-0.4f);
    const Mat4 mvp = proj * view * model;

    for (const Tri& t : kTris) {
        SVert s[3];
        bool ok = true;
        for (int i = 0; i < 3; ++i) {
            const Vec3& p = kCube[t.idx[i]];
            const Vec4 clip = mvp * Vec4{p.x, p.y, p.z, 1.0f};
            if (clip.w <= 1e-5f) { ok = false; break; }  // 顶点在相机后面，整三角形丢弃
            s[i] = toScreen(clip, kWidth, kHeight);
        }
        if (ok) drawTriangle(fb, s[0], s[1], s[2], t.color, kCullBack);
    }
}

}  // namespace

int main() {
    try {
        Framebuffer fb(kWidth, kHeight);
        drawCube(fb);

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
