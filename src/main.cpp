#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/framebuffer.h"
#include "core/texture.h"
#include "math/mat.h"
#include "math/vec.h"
#include "pipeline/rasterizer.h"
#include "pipeline/shader.h"
#include "pipeline/vertex.h"
#include "platform/window_sdl.h"

namespace {

constexpr int kWidth = 960;
constexpr int kHeight = 540;
constexpr float kPi = 3.14159265358979323846f;

constexpr bool kCullBack = true;
constexpr bool kPerspectiveCorrect = true;

const Vec3 kEye{2.8f, 2.1f, 3.8f};
const Vec3 kTarget{0.0f, 0.0f, 0.0f};
const Vec3 kUp{0.0f, 1.0f, 0.0f};

// 棋盘格纹理：size×size 个纹素，每格 cell×cell 个纹素
Texture makeCheckerboard(int size, int cell) {
    Texture t(size, size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const int cx = x / cell, cy = y / cell;
            t.setTexel(x, y, ((cx + cy) & 1) ? rgb(122, 52, 40) : rgb(222, 210, 186));
        }
    return t;
}

const Texture g_checker = makeCheckerboard(64, 8);

struct Mesh {
    std::vector<Vertex> verts;
    std::vector<int> indices;   // 每 3 个下标一个三角形
};

// 立方体：棱长 2、中心在原点、CCW 朝外。
// 每个面单独 4 个顶点 —— 法线不能跨面共享，否则立方体边缘会被"磨圆"。
Mesh makeCube() {
    const Vec3 n[6] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    const Vec3 p[6][4] = {
        {{-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}},  // +Z 前
        {{ 1, -1, -1}, {-1, -1, -1}, {-1,  1, -1}, { 1,  1, -1}},  // -Z 后
        {{ 1, -1,  1}, { 1, -1, -1}, { 1,  1, -1}, { 1,  1,  1}},  // +X 右
        {{-1, -1, -1}, {-1, -1,  1}, {-1,  1,  1}, {-1,  1, -1}},  // -X 左
        {{-1,  1,  1}, { 1,  1,  1}, { 1,  1, -1}, {-1,  1, -1}},  // +Y 上
        {{-1, -1, -1}, { 1, -1, -1}, { 1, -1,  1}, {-1, -1,  1}},  // -Y 下
    };
    // 每面 uv 只占 [0,0.5]²，这样每个面铺 4×4 格（整张贴图是 8×8 格）
    const Vec2 uv[4] = {{0, 0}, {0.5f, 0}, {0.5f, 0.5f}, {0, 0.5f}};

    Mesh m;
    for (int f = 0; f < 6; ++f) {
        const int base = int(m.verts.size());
        for (int i = 0; i < 4; ++i) m.verts.push_back({p[f][i], n[f], uv[i]});
        m.indices.insert(m.indices.end(), {base + 0, base + 1, base + 2,
                                           base + 0, base + 2, base + 3});
    }
    return m;
}

// 地面：y = -1 的 4×4 四边形，法线朝上
Mesh makeFloor() {
    Mesh m;
    m.verts = {
        {{-2, -1, -2}, {0, 1, 0}, {0, 0}},
        {{ 2, -1, -2}, {0, 1, 0}, {1, 0}},
        {{ 2, -1,  2}, {0, 1, 0}, {1, 1}},
        {{-2, -1,  2}, {0, 1, 0}, {0, 1}},
    };
    m.indices = {0, 2, 1, 0, 3, 2};   // 绕序要让它屏幕上是正面（area > 0）
    return m;
}

// UV 球：法线就是单位化后的位置，所以表面是"光滑"的 —— 高光才能连成一片
Mesh makeSphere(float radius, int rings, int segs) {
    Mesh m;
    for (int r = 0; r <= rings; ++r) {
        const float phi = kPi * float(r) / float(rings);   // 0=北极 .. π=南极
        for (int s = 0; s <= segs; ++s) {
            const float th = 2.0f * kPi * float(s) / float(segs);
            const Vec3 n{std::sin(phi) * std::cos(th), std::cos(phi), std::sin(phi) * std::sin(th)};
            m.verts.push_back({n * radius, n, {float(s) / float(segs), float(r) / float(rings)}});
        }
    }
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segs; ++s) {
            const int a = r * (segs + 1) + s;
            const int b = a + segs + 1;
            m.indices.insert(m.indices.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    }
    return m;
}

const Mesh g_cube = makeCube();
const Mesh g_floor = makeFloor();
const Mesh g_sphere = makeSphere(0.62f, 24, 32);

// 顶点处理 + 图元装配：把 Mesh 里的每三个下标变成一个三角形
void drawMesh(Framebuffer& fb, const Mesh& mesh, const Mat4& model, const IShader& shader) {
    const Mat4 view = lookAt(kEye, kTarget, kUp);
    const Mat4 proj = perspective(60.0f * kPi / 180.0f, float(kWidth) / float(kHeight), 0.1f, 100.0f);
    const Mat4 mvp = proj * view * model;
    const Mat3 nrmMat = normalMatrix(model);

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        SVert s[3];
        bool ok = true;
        for (int k = 0; k < 3; ++k) {
            const Vertex& v = mesh.verts[size_t(mesh.indices[i + size_t(k)])];
            const Vec4 pos{v.position.x, v.position.y, v.position.z, 1.0f};
            const Vec4 wpos = model * pos;

            VSOut o;
            o.clip = mvp * pos;
            o.worldPos = {wpos.x, wpos.y, wpos.z};
            o.normal = normalize(nrmMat * v.normal);
            o.uv = v.uv;

            // D3 还没做近平面裁剪：只要有顶点跑到相机背后，就整片丢掉
            if (o.clip.w <= 1e-5f) { ok = false; break; }
            s[k] = toScreen(o, kWidth, kHeight);
        }
        if (ok) drawTriangle(fb, s[0], s[1], s[2], kCullBack, kPerspectiveCorrect, shader);
    }
}

// 命令行开关，用来做对照实验（README 里的"踩坑图"）
struct Options {
    bool specular = true;        // --no-spec
    bool normalizeNormal = true; // --no-normalize
};

void drawScene(Framebuffer& fb, float t, const Options& opt) {
    fb.clear(rgb(16, 16, 24));

    // 贴图材质：地面和立方体用
    PhongShader textured(&g_checker, kEye);
    textured.light.position = {2.5f, 3.5f, 4.0f};
    textured.light.intensity = 1.15f;
    textured.useSpecular = opt.specular;
    textured.normalizeNormal = opt.normalizeNormal;

    // 纯色材质：球体用（没有贴图，高光看得最清楚）
    PhongShader solid(nullptr, kEye);
    solid.albedo = {0.16f, 0.34f, 0.72f};
    solid.light = textured.light;
    solid.shininess = 64.0f;
    solid.specStrength = 0.9f;
    solid.useSpecular = opt.specular;
    solid.normalizeNormal = opt.normalizeNormal;

    drawMesh(fb, g_floor, Mat4::identity(), textured);

    // 先绕 X 歪一点，再绕 Y 转 —— 相当于 model = Ry * Rx，右乘先作用
    const Mat4 model = rotationY(0.7f * t) * rotationX(0.35f);
    drawMesh(fb, g_cube, model, textured);

    // 球体：贴着地面滚不到的地方安静待着，专门展示 Blinn-Phong 高光
    drawMesh(fb, g_sphere, translation({1.95f, -0.38f, 0.55f}), solid);
}

}  // namespace

int main(int argc, char** argv) {
    bool once = false;
    float time = 0.0f;
    Options opt;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--once") == 0) {
            once = true;
        } else if (std::strcmp(argv[i], "--time") == 0 && i + 1 < argc) {
            time = float(std::atof(argv[++i]));
        } else if (std::strcmp(argv[i], "--no-spec") == 0) {
            opt.specular = false;
        } else if (std::strcmp(argv[i], "--no-normalize") == 0) {
            opt.normalizeNormal = false;
        } else {
            std::fprintf(stderr, "用法: %s [--once] [--time <秒>] [--no-spec] [--no-normalize]\n", argv[0]);
            return 1;
        }
    }

    try {
        Framebuffer fb(kWidth, kHeight);

        // 无窗口渲一帧就退：方便脚本出图 / 以后写回归测试
        if (once) {
            drawScene(fb, time, opt);
            if (fb.exportTGA("out.tga")) std::puts("已导出 out.tga");
            return 0;
        }

        Window window("soft raster | D3", kWidth, kHeight);
        std::puts("按 S 重新导出 out.tga，ESC 或关窗退出。");

        const auto start = std::chrono::steady_clock::now();
        auto fpsTimer = start;
        int frames = 0;
        bool running = true;

        while (running) {
            const FrameInput in = window.pollEvents();
            if (in.quit) running = false;
            if (in.saveRequested && fb.exportTGA("out.tga")) std::puts("已导出 out.tga");

            const auto now = std::chrono::steady_clock::now();
            const float t = std::chrono::duration<float>(now - start).count();

            drawScene(fb, t, opt);   // 每帧重画，立方体才会转
            window.present(fb);

            ++frames;
            const double elapsed = std::chrono::duration<double>(now - fpsTimer).count();
            if (elapsed >= 0.5) {
                char title[64];
                std::snprintf(title, sizeof(title), "soft raster | D3 | %.0f FPS", frames / elapsed);
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
