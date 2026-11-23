// D6：benchmark 骨架。指导 §10.1 的三条要求：
//   1. 每个配置跑 30 帧、丢掉前 3 帧预热，报中位数（不报最好成绩）
//   2. 同时统计三角形数、覆盖片元、着色片元 → 算出 overdraw
//   3. 分阶段计时：顶点 / 光栅
// 输出 CSV 到 stdout（或 --csv 指定的文件），方便脚本画图。

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/framebuffer.h"
#include "math/mat.h"
#include "pipeline/renderer.h"
#include "scene/camera.h"
#include "scene/demo_scene.h"

namespace {

constexpr float kPi = 3.14159265358979323846f;
using Clock = std::chrono::steady_clock;

struct Resolution {
    int w;
    int h;
};

struct Variant {
    const char* name;
    bool incrementalEdges;
    bool cullBack;
    bool flatShade;   // 用常量着色器替换材质：差值 = 片元着色的成本
};

// 只返回常量颜色。它和 PhongShader 的耗时差就是"着色"这一段的代价。
class FlatShader : public IShader {
public:
    Color fragment(const Fragment&) const override { return rgb(200, 200, 200); }
};

double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t i = size_t(p * double(v.size() - 1) + 0.5);
    if (i >= v.size()) i = v.size() - 1;
    return v[i];
}

struct Result {
    double msMedian = 0.0, msP05 = 0.0, msP95 = 0.0;
    RenderStats stats;
};

Result runCase(int w, int h, const Variant& variant, int frames, int warmup) {
    Framebuffer fb(w, h);
    DemoScene scene;
    std::string err;
    if (!scene.loadModel("assets/torus.obj", &err))
        std::fprintf(stderr, "警告：%s，回退程序化立方体\n", err.c_str());

    OrbitCamera cam;
    cam.target = {0.0f, -0.6f, 0.0f};

    RenderOptions opt;
    opt.incrementalEdges = variant.incrementalEdges;
    opt.cullBack = variant.cullBack;

    const FlatShader flat;

    const Mat4 proj = perspective(60.0f * kPi / 180.0f, float(w) / float(h), 0.1f, 100.0f);

    Result result;
    std::vector<double> times;
    times.reserve(size_t(frames));

    for (int i = 0; i < warmup + frames; ++i) {
        // 时间参数每帧推进一点：缓存状态、模型矩阵都在动，避免"测到不变的东西"
        const float t = 0.01f * float(i);
        fb.clear(rgb(12, 14, 20));
        scene.update(t, cam);

        std::vector<DrawCall> calls = scene.calls();
        if (variant.flatShade)
            for (DrawCall& c : calls) c.shader = &flat;

        const Clock::time_point t0 = Clock::now();
        renderScene(fb, calls, proj * cam.view(), opt, &result.stats);
        const Clock::time_point t1 = Clock::now();

        if (i >= warmup)
            times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    std::vector<double> sorted = times;
    std::sort(sorted.begin(), sorted.end());
    result.msMedian = sorted[sorted.size() / 2];
    result.msP05 = percentile(times, 0.05);
    result.msP95 = percentile(times, 0.95);
    return result;
}

void printCsvHeader(std::FILE* out) {
    std::fprintf(out,
                 "scene,width,height,variant,ms_median,ms_p05,ms_p95,fps,triangles,culled,emitted,"
                 "covered,shaded,overdraw,ms_vertex,ms_raster\n");
}

void printCsvRow(std::FILE* out, const char* sceneName, int w, int h, const Variant& v,
                 const Result& r) {
    const RenderStats& s = r.stats;
    std::fprintf(out, "%s,%d,%d,%s,%.3f,%.3f,%.3f,%.1f,%zu,%zu,%zu,%llu,%llu,%.3f,%.3f,%.3f\n",
                 sceneName, w, h, v.name, r.msMedian, r.msP05, r.msP95, 1000.0 / r.msMedian,
                 s.triangles, s.culled, s.emitted, s.covered, s.shaded, s.overdraw(), s.msVertex,
                 s.msRaster);
}

}  // namespace

int main(int argc, char** argv) {
    int frames = 30;
    int warmup = 3;
    std::string csvPath;
    std::vector<Resolution> resolutions = {{960, 540}, {1280, 720}, {1920, 1080}};

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            warmup = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
            csvPath = argv[++i];
        } else if (std::strcmp(argv[i], "--res") == 0 && i + 1 < argc) {
            int w = 0, h = 0;
            if (std::sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
                resolutions = {{w, h}};
            }
        } else {
            std::fprintf(stderr,
                         "用法: %s [--frames 30] [--warmup 3] [--csv <path>] [--res 1280x720]\n",
                         argv[0]);
            return 1;
        }
    }

    // 所有 variant 跑同一份场景，每次只改一个开关
    const Variant variants[] = {
        {"naive", false, true, false},        // D2 写法：每像素重算三次边函数
        {"incremental", true, true, false},   // D6：增量边函数
        {"no-cull", true, false, false},      // 关掉背面剔除，量它的收益
        {"flat-shade", true, true, true},     // 常量着色器：和 incremental 的差 = 着色成本
    };

    std::FILE* csv = stdout;
    if (!csvPath.empty()) {
        csv = std::fopen(csvPath.c_str(), "w");
        if (!csv) {
            std::fprintf(stderr, "无法写入 %s\n", csvPath.c_str());
            return 1;
        }
    }

    printCsvHeader(csv);

    std::fprintf(stderr, "%-12s %-11s %-12s %8s %8s %8s %8s %8s\n", "scene", "res", "variant",
                 "median", "p05", "p95", "vertex", "raster");

    for (const Resolution& res : resolutions) {
        for (const Variant& v : variants) {
            const Result r = runCase(res.w, res.h, v, frames, warmup);
            printCsvRow(csv, "demo", res.w, res.h, v, r);
            std::fflush(csv);
            std::fprintf(stderr, "%-12s %4dx%-6d %-12s %7.2fms %7.2f %7.2f %7.2f %7.2f\n", "demo",
                         res.w, res.h, v.name, r.msMedian, r.msP05, r.msP95, r.stats.msVertex,
                         r.stats.msRaster);
        }
    }

    if (csv != stdout) std::fclose(csv);
    return 0;
}
