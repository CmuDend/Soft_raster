#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "core/framebuffer.h"
#include "math/mat.h"
#include "pipeline/renderer.h"
#include "platform/window_sdl.h"
#include "scene/camera.h"
#include "scene/demo_scene.h"

namespace {

constexpr int kWidth = 960;
constexpr int kHeight = 540;
constexpr float kPi = 3.14159265358979323846f;

Mat4 makeViewProj(const OrbitCamera& cam, int width, int height) {
    const Mat4 proj =
        perspective(60.0f * kPi / 180.0f, float(width) / float(height), 0.1f, 100.0f);
    return proj * cam.view();
}

// 命令行开关，用来做对照实验
struct Options {
    bool specular = true;          // --no-spec
    bool normalizeNormal = true;   // --no-normalize
    bool clipNear = true;          // --no-clip：退回 D4 的"整片丢弃"做对照
    bool incrementalEdges = true;  // --no-incremental：退回 D2 的逐像素边函数
};

RenderOptions toRenderOptions(const Options& o) {
    RenderOptions r;
    r.clipNear = o.clipNear;
    r.incrementalEdges = o.incrementalEdges;
    return r;
}

void drawScene(Framebuffer& fb, DemoScene& scene, const OrbitCamera& cam, float t,
               const Options& opt, RenderStats* stats = nullptr) {
    fb.clear(rgb(12, 14, 20));

    scene.setOptions({opt.specular, opt.normalizeNormal});
    scene.update(t, cam);

    const Mat4 viewProj = makeViewProj(cam, fb.width(), fb.height());
    renderScene(fb, scene.calls(), viewProj, toRenderOptions(opt), stats);
}

}  // namespace

int main(int argc, char** argv) {
    bool once = false;
    int benchFrames = 0;
    float time = 0.0f;
    std::string modelPath = "assets/torus.obj";
    Options opt;
    OrbitCamera cam;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--once") == 0) {
            once = true;
        } else if (std::strcmp(argv[i], "--bench") == 0 && i + 1 < argc) {
            benchFrames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--time") == 0 && i + 1 < argc) {
            time = float(std::atof(argv[++i]));
        } else if (std::strcmp(argv[i], "--no-spec") == 0) {
            opt.specular = false;
        } else if (std::strcmp(argv[i], "--no-normalize") == 0) {
            opt.normalizeNormal = false;
        } else if (std::strcmp(argv[i], "--no-clip") == 0) {
            opt.clipNear = false;
        } else if (std::strcmp(argv[i], "--no-incremental") == 0) {
            opt.incrementalEdges = false;
        } else if (std::strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            modelPath = argv[++i];
        } else if (std::strcmp(argv[i], "--yaw") == 0 && i + 1 < argc) {
            cam.yaw = float(std::atof(argv[++i]));
        } else if (std::strcmp(argv[i], "--pitch") == 0 && i + 1 < argc) {
            cam.pitch = float(std::atof(argv[++i]));
        } else if (std::strcmp(argv[i], "--dist") == 0 && i + 1 < argc) {
            cam.distance = float(std::atof(argv[++i]));
        } else {
            std::fprintf(stderr,
                         "用法: %s [--once] [--bench <帧>] [--time <秒>] [--model <obj>] "
                         "[--yaw <rad>] [--pitch <rad>] [--dist <d>] [--no-spec] [--no-normalize] "
                         "[--no-clip] [--no-incremental]\n",
                         argv[0]);
            return 1;
        }
    }

    try {
        Framebuffer fb(kWidth, kHeight);
        cam.target = {0.0f, -0.6f, 0.0f};

        DemoScene scene;
        std::string err;
        if (scene.loadModel(modelPath, &err)) {
            std::printf("已加载 %s：%zu 顶点，%zu 三角形\n", modelPath.c_str(),
                        scene.model().verts.size(), scene.model().triangleCount());
        } else {
            std::fprintf(stderr, "加载 OBJ 失败（%s），改用程序化立方体\n", err.c_str());
        }

        // 无窗口连渲 N 帧：量的是纯渲染时间，不含开窗和 present
        if (benchFrames > 0) {
            RenderStats stats;
            drawScene(fb, scene, cam, 0.5f, opt, &stats);  // 预热，排除首次缺页

            // 逐帧计时取中位数：均值会被一次调度抖动拖偏（指导 §10.1）
            std::vector<double> times;
            times.reserve(size_t(benchFrames));
            for (int i = 0; i < benchFrames; ++i) {
                const auto t0 = std::chrono::steady_clock::now();
                drawScene(fb, scene, cam, 0.1f * float(i), opt, &stats);
                const auto t1 = std::chrono::steady_clock::now();
                times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
            }
            std::sort(times.begin(), times.end());
            const double ms = times[times.size() / 2];
            std::printf("%d 帧中位数 %.2f ms/帧（%.0f FPS，%zu 三角形，%dx%d）\n", benchFrames, ms,
                        1000.0 / ms, scene.triangleCount(), kWidth, kHeight);
            std::printf("  顶点 %.2f ms + 光栅 %.2f ms | 覆盖 %llu 片元（%.2f/像素），着色 %llu"
                        "（%.0f%% 通过深度测试）\n",
                        stats.msVertex, stats.msRaster, stats.covered, stats.overdraw(),
                        stats.shaded, 100.0 * stats.shadeRatio());
            return 0;
        }

        // 无窗口渲一帧就退：方便脚本出图 / 以后写回归测试
        if (once) {
            drawScene(fb, scene, cam, time, opt);
            if (fb.exportTGA("out.tga")) std::puts("已导出 out.tga");
            return 0;
        }

        Window window("soft raster | D6", kWidth, kHeight);
        std::puts("左键拖动旋转，滚轮缩放，R 复位，S 存图，ESC 退出。");

        const auto start = std::chrono::steady_clock::now();
        auto fpsTimer = start;
        int frames = 0;

        bool running = true;
        while (running) {
            const FrameInput in = window.pollEvents();
            if (in.quit) running = false;
            if (in.saveRequested && fb.exportTGA("out.tga")) std::puts("已导出 out.tga");
            if (in.resetView) cam.reset();
            if (in.mouseDX != 0.0f || in.mouseDY != 0.0f) cam.orbit(in.mouseDX, in.mouseDY);
            if (in.wheel != 0.0f) cam.zoom(in.wheel);

            const auto now = std::chrono::steady_clock::now();
            const float t = std::chrono::duration<float>(now - start).count();

            drawScene(fb, scene, cam, t, opt);
            window.present(fb);

            ++frames;
            const double elapsed = std::chrono::duration<double>(now - fpsTimer).count();
            if (elapsed >= 0.5) {
                char title[128];
                std::snprintf(title, sizeof(title), "soft raster | D6 | %.0f FPS | %zu tris",
                              frames / elapsed, scene.triangleCount());
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
