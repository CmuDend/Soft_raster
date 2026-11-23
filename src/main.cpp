#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "core/framebuffer.h"
#include "core/texture.h"
#include "math/mat.h"
#include "math/vec.h"
#include "pipeline/clip.h"
#include "pipeline/rasterizer.h"
#include "pipeline/shader.h"
#include "platform/window_sdl.h"
#include "scene/camera.h"
#include "scene/mesh.h"
#include "scene/obj_loader.h"

namespace {

constexpr int kWidth = 960;
constexpr int kHeight = 540;
constexpr float kPi = 3.14159265358979323846f;

constexpr bool kCullBack = true;
constexpr bool kPerspectiveCorrect = true;

// 棋盘格纹理：size×size 个纹素，每格 cell×cell 个纹素
Texture makeCheckerboard(int size, int cell) {
    Texture t(size, size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const int cx = x / cell, cy = y / cell;
            t.setTexel(x, y, ((cx + cy) & 1) ? rgb(122, 52, 40) : rgb(222, 210, 186));
        }
    t.setWrap(Wrap::Repeat);   // 地面 uv 会超出 [0,1]，要平铺
    return t;
}

const Texture g_checker = makeCheckerboard(64, 8);

const Mesh g_floor = makeFloor(-1.35f, 20.0f, 32);
const Mesh g_sphere = makeSphere(0.62f, 24, 32);
const Mesh g_cube = makeCube();

// 顶点处理 + 图元装配。viewProj 由调用方传进来，一帧只算一次。
void drawMesh(Framebuffer& fb, const Mesh& mesh, const Mat4& model, const Mat4& viewProj,
              const IShader& shader, bool clipNear = true) {
    const Mat4 mvp = viewProj * model;
    const Mat3 nrmMat = normalMatrix(model);

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        VSOut tri[3];
        for (int k = 0; k < 3; ++k) {
            const Vertex& v = mesh.verts[size_t(mesh.indices[i + size_t(k)])];
            const Vec4 pos{v.position.x, v.position.y, v.position.z, 1.0f};
            const Vec4 wpos = model * pos;

            VSOut& o = tri[k];
            o.clip = mvp * pos;
            o.worldPos = {wpos.x, wpos.y, wpos.z};
            o.normal = normalize(nrmMat * v.normal);
            o.uv = v.uv;
        }

        // 近平面裁剪：跨近平面的三角形在这里被切开，而不是整片丢弃（指导 §7.3）
        VSOut poly[4];
        int n = 0;
        if (clipNear) {
            n = clipTriangleNear(tri, poly);
        } else {
            // 对照实验：D4 的做法，有顶点在近平面后就整片丢弃
            if (tri[0].clip.w > 1e-5f && tri[1].clip.w > 1e-5f && tri[2].clip.w > 1e-5f) {
                poly[0] = tri[0];
                poly[1] = tri[1];
                poly[2] = tri[2];
                n = 3;
            }
        }
        if (n < 3) continue;

        // 扇形三角化：n == 3 时就是原来那一个三角形，n == 4 时切成两个
        SVert s[4];
        for (int k = 0; k < n; ++k) s[k] = toScreen(poly[k], kWidth, kHeight);
        for (int k = 0; k + 2 < n; ++k) {
            drawTriangle(fb, s[0], s[k + 1], s[k + 2], kCullBack, kPerspectiveCorrect, shader);
        }
    }
}

// 命令行开关，用来做对照实验
struct Options {
    bool specular = true;         // --no-spec
    bool normalizeNormal = true;  // --no-normalize
    bool clipNear = true;         // --no-clip：退回 D4 的"整片丢弃"做对照
};

void drawScene(Framebuffer& fb, const OrbitCamera& cam, const Mesh& model, float t,
               const Options& opt) {
    fb.clear(rgb(12, 14, 20));

    const Mat4 proj = perspective(60.0f * kPi / 180.0f, float(kWidth) / float(kHeight), 0.1f, 100.0f);
    const Mat4 viewProj = proj * cam.view();
    const Vec3 eye = cam.eye();

    // 地面：贴图 + 弱高光（太强会一片惨白）
    PhongShader floorMat(&g_checker, eye);
    floorMat.light.position = {3.5f, 6.0f, 4.0f};
    floorMat.light.intensity = 1.0f;
    floorMat.specStrength = 0.18f;
    floorMat.shininess = 32.0f;
    floorMat.useSpecular = opt.specular;
    floorMat.normalizeNormal = opt.normalizeNormal;

    // 圆环：贴图 + 强高光
    PhongShader torusMat(&g_checker, eye);
    torusMat.light = floorMat.light;
    torusMat.specStrength = 0.9f;
    torusMat.shininess = 48.0f;
    torusMat.useSpecular = opt.specular;
    torusMat.normalizeNormal = opt.normalizeNormal;

    // 纯色材质：球体（高光最清楚）
    PhongShader solid(nullptr, eye);
    solid.albedo = {0.16f, 0.34f, 0.72f};
    solid.light = floorMat.light;
    solid.shininess = 64.0f;
    solid.specStrength = 0.9f;
    solid.useSpecular = opt.specular;
    solid.normalizeNormal = opt.normalizeNormal;

    drawMesh(fb, g_floor, Mat4::identity(), viewProj, floorMat, opt.clipNear);

    // 圆环：先绕 X 倾斜，再绕 Y 转（右乘先作用），浮在地面上方
    const Mat4 torusModel =
        translation({0.0f, -0.5f, 0.0f}) * rotationY(0.6f * t) * rotationX(0.42f);
    drawMesh(fb, model, torusModel, viewProj, torusMat, opt.clipNear);

    drawMesh(fb, g_sphere, translation({2.05f, -0.73f, 0.85f}), viewProj, solid, opt.clipNear);
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
                         "[--no-clip]\n",
                         argv[0]);
            return 1;
        }
    }

    try {
        Framebuffer fb(kWidth, kHeight);
        cam.target = {0.0f, -0.6f, 0.0f};

        // 模型加载失败就退回程序化立方体 —— 指导 §13 D4 的降级方案
        Mesh model;
        std::string err;
        if (loadOBJ(modelPath, model, &err)) {
            std::printf("已加载 %s：%zu 顶点，%zu 三角形\n", modelPath.c_str(), model.verts.size(),
                        model.triangleCount());
        } else {
            std::fprintf(stderr, "加载 OBJ 失败（%s），改用程序化立方体\n", err.c_str());
            model = g_cube;
        }

        // 无窗口连渲 N 帧：量的是纯渲染时间，不含开窗和 present
        if (benchFrames > 0) {
            drawScene(fb, cam, model, 0.5f, opt);   // 预热，把首次缺页之类排除掉
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < benchFrames; ++i) drawScene(fb, cam, model, 0.1f * float(i), opt);
            const auto t1 = std::chrono::steady_clock::now();
            const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / benchFrames;
            std::printf("%d 帧，平均 %.2f ms/帧（%.0f FPS，%zu 三角形，%dx%d）\n", benchFrames, ms,
                        1000.0 / ms, model.triangleCount() + g_floor.triangleCount() +
                                         g_sphere.triangleCount(),
                        kWidth, kHeight);
            return 0;
        }

        // 无窗口渲一帧就退：方便脚本出图 / 以后写回归测试
        if (once) {
            drawScene(fb, cam, model, time, opt);
            if (fb.exportTGA("out.tga")) std::puts("已导出 out.tga");
            return 0;
        }

        Window window("soft raster | D4", kWidth, kHeight);
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

            drawScene(fb, cam, model, t, opt);
            window.present(fb);

            ++frames;
            const double elapsed = std::chrono::duration<double>(now - fpsTimer).count();
            if (elapsed >= 0.5) {
                char title[128];
                std::snprintf(title, sizeof(title), "soft raster | D4 | %.0f FPS | %zu tris",
                              frames / elapsed, model.triangleCount());
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
