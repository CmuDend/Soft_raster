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

// 立方体 8 顶点 + 12 条边（这是数据，不是 D1 的考点，直接给全）
const Vec3 kCube[8] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},
};
const int kEdges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},  // 背面 z=-1
    {4, 5}, {5, 6}, {6, 7}, {7, 4},  // 正面 z=+1
    {0, 4}, {1, 5}, {2, 6}, {3, 7},  // 竖边
};

// 把立方体经 MVP 变换投到屏幕，画 12 条边；同时打印 NDC 供你对手算
void drawWireframe(Framebuffer& fb) {
    fb.clear(rgb(16, 16, 24));

    const Mat4 view = lookAt({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    const Mat4 proj = perspective(60.0f * kPi / 180.0f, float(kWidth) / float(kHeight), 0.1f, 100.0f);
    const Mat4 mvp = proj * view;

    SVert screen[8];
    bool valid[8] = {};
    for (int i = 0; i < 8; ++i) {
        const Vec4 clip = mvp * Vec4{kCube[i].x, kCube[i].y, kCube[i].z, 1.0f};
        if (clip.w <= 1e-5f) {  // 顶点在相机平面之后，D1 直接跳过
            std::printf("v%d 在相机后面，跳过\n", i);
            continue;
        }
        valid[i] = true;
        screen[i] = toScreen(clip, kWidth, kHeight);

        const float ndcX = clip.x / clip.w;
        const float ndcY = clip.y / clip.w;
        const float ndcZ = clip.z / clip.w;
        std::printf("v%d (%+g,%+g,%+g) -> ndc(%.3f, %.3f, %.3f) screen(%.1f, %.1f)\n",
                    i, kCube[i].x, kCube[i].y, kCube[i].z, ndcX, ndcY, ndcZ,
                    screen[i].x, screen[i].y);
    }

    for (int e = 0; e < 12; ++e) {
        const int ia = kEdges[e][0], ib = kEdges[e][1];
        if (!valid[ia] || !valid[ib]) continue;
        drawLine(fb, screen[ia], screen[ib], rgb(200, 210, 230));
    }
}

}  // namespace

int main() {
    try {
        Framebuffer fb(kWidth, kHeight);
        drawWireframe(fb);

        if (fb.exportTGA("out.tga")) {
            std::puts("已导出 out.tga");
        } else {
            std::fprintf(stderr, "导出 out.tga 失败\n");
        }

        Window window("soft raster | D1", kWidth, kHeight);
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
                std::snprintf(title, sizeof(title), "soft raster | D1 | %.0f FPS", frames / elapsed);
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
