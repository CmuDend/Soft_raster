// soft_raster —— 从零开始的软件光栅化器
//
// 管线：模型空间 --MVP--> 裁剪空间 --透视除法--> NDC --视口变换--> 屏幕空间
//       三角形 --边函数--> 覆盖的像素 --重心坐标--> 深度测试 + 透视校正插值颜色
//
// 编译：cmake --build build
// 运行：./build/soft_raster                              （开窗口，实时旋转）
//      ./build/soft_raster --dump out.ppm --time 0.9    （离屏渲染一帧，便于对比/自动化）

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ================================================================ 数学库

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator*(float s, Vec3 a) { return a * s; }
inline Vec3& operator+=(Vec3& a, Vec3 b) { a = a + b; return a; }

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }

inline Vec3 normalize(Vec3 a) {
    const float len = length(a);
    return len > 1e-8f ? a * (1.0f / len) : Vec3{};
}

inline Vec3 mix(Vec3 a, Vec3 b, float t) { return a * (1.0f - t) + b * t; }

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(Vec3 v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
};

// 4x4 矩阵，行主序，列向量约定：v' = M * v
struct Mat4 {
    float m[4][4] = {};

    static Mat4 identity() {
        Mat4 r;
        for (int i = 0; i < 4; ++i) r.m[i][i] = 1.0f;
        return r;
    }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.m[i][k] * b.m[k][j];
            r.m[i][j] = sum;
        }
    return r;
}

inline Vec4 operator*(const Mat4& a, const Vec4& v) {
    return {a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z + a.m[0][3] * v.w,
            a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z + a.m[1][3] * v.w,
            a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z + a.m[2][3] * v.w,
            a.m[3][0] * v.x + a.m[3][1] * v.y + a.m[3][2] * v.z + a.m[3][3] * v.w};
}

// 把点（w=1）变换过去，返回齐次坐标
inline Vec3 transformPoint(const Mat4& m, Vec3 p) {
    return {m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2] * p.z + m.m[0][3],
            m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2] * p.z + m.m[1][3],
            m.m[2][0] * p.x + m.m[2][1] * p.y + m.m[2][2] * p.z + m.m[2][3]};
}

inline Mat4 translation(Vec3 t) {
    Mat4 r = Mat4::identity();
    r.m[0][3] = t.x;
    r.m[1][3] = t.y;
    r.m[2][3] = t.z;
    return r;
}

inline Mat4 scaling(float s) {
    Mat4 r = Mat4::identity();
    r.m[0][0] = r.m[1][1] = r.m[2][2] = s;
    return r;
}

inline Mat4 rotationX(float rad) {
    const float c = std::cos(rad), s = std::sin(rad);
    Mat4 r = Mat4::identity();
    r.m[1][1] = c;  r.m[1][2] = -s;
    r.m[2][1] = s;  r.m[2][2] = c;
    return r;
}

inline Mat4 rotationY(float rad) {
    const float c = std::cos(rad), s = std::sin(rad);
    Mat4 r = Mat4::identity();
    r.m[0][0] = c;   r.m[0][2] = s;
    r.m[2][0] = -s;  r.m[2][2] = c;
    return r;
}

// 透视投影：视线朝 -Z，NDC 的 z ∈ [-1,1]，近平面 -> -1，远平面 -> +1
inline Mat4 perspective(float fovyRad, float aspect, float zNear, float zFar) {
    const float f = 1.0f / std::tan(fovyRad * 0.5f);
    Mat4 r;
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = (zFar + zNear) / (zNear - zFar);
    r.m[2][3] = (2.0f * zFar * zNear) / (zNear - zFar);
    r.m[3][2] = -1.0f;
    return r;
}

inline Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) {
    const Vec3 f = normalize(target - eye);  // 前
    const Vec3 s = normalize(cross(f, up));  // 右
    const Vec3 u = cross(s, f);              // 上
    Mat4 r = Mat4::identity();
    r.m[0][0] = s.x;  r.m[0][1] = s.y;  r.m[0][2] = s.z;  r.m[0][3] = -dot(s, eye);
    r.m[1][0] = u.x;  r.m[1][1] = u.y;  r.m[1][2] = u.z;  r.m[1][3] = -dot(u, eye);
    r.m[2][0] = -f.x; r.m[2][1] = -f.y; r.m[2][2] = -f.z; r.m[2][3] = dot(f, eye);
    return r;
}

// Vec3 分量 ∈ [0,1] -> 0xAARRGGBB
inline uint32_t packColor(Vec3 c) {
    auto toByte = [](float v) {
        v = std::min(1.0f, std::max(0.0f, v));
        return uint32_t(v * 255.0f + 0.5f);
    };
    return 0xFF000000u | (toByte(c.x) << 16) | (toByte(c.y) << 8) | toByte(c.z);
}

// ================================================================ 帧缓冲

struct Framebuffer {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> color;  // 0xAARRGGBB
    std::vector<float> depth;     // [0,1]，0 = 近，1 = 远

    Framebuffer(int w, int h)
        : width(w), height(h), color(size_t(w) * size_t(h)), depth(size_t(w) * size_t(h)) {}

    uint32_t* colorRow(int y) { return color.data() + size_t(y) * size_t(width); }
    float* depthRow(int y) { return depth.data() + size_t(y) * size_t(width); }
    const uint32_t* colorRow(int y) const { return color.data() + size_t(y) * size_t(width); }

    void clearDepth() { std::fill(depth.begin(), depth.end(), 1.0f); }
};

// 超采样：把高分辨率帧缓冲按整数倍做盒式降采样，得到抗锯齿结果
void downsampleBox(const Framebuffer& src, Framebuffer& dst) {
    const int factor = src.width / dst.width;
    if (factor <= 0) return;
    const uint32_t n = static_cast<uint32_t>(factor * factor);

    for (int y = 0; y < dst.height; ++y) {
        uint32_t* drow = dst.colorRow(y);
        for (int x = 0; x < dst.width; ++x) {
            uint32_t r = 0, g = 0, b = 0;
            for (int sy = 0; sy < factor; ++sy) {
                const uint32_t* srow = src.colorRow(y * factor + sy) + x * factor;
                for (int sx = 0; sx < factor; ++sx) {
                    const uint32_t c = srow[sx];
                    r += (c >> 16) & 0xFFu;
                    g += (c >> 8) & 0xFFu;
                    b += c & 0xFFu;
                }
            }
            drow[x] = 0xFF000000u | ((r / n) << 16) | ((g / n) << 8) | (b / n);
        }
    }
}

// ================================================================ 光栅化

// 边的有向面积（2 倍）：p 在 AB 左侧为正
inline float edgeFunction(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

struct ScreenVert {
    float x = 0.0f, y = 0.0f;  // 屏幕像素坐标（y 向下）
    float z = 0.0f;            // NDC 深度 [-1,1]
    float invW = 1.0f;         // 1 / clip.w，透视校正插值用
    Vec3 color;                // 顶点色
};

// 画线段（Bresenham），线框模式用
inline void drawLine(Framebuffer& fb, int x0, int y0, int x1, int y1, uint32_t col) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (x0 >= 0 && x0 < fb.width && y0 >= 0 && y0 < fb.height) fb.colorRow(y0)[x0] = col;
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// 单个三角形：边函数判覆盖 + 重心坐标 + 深度测试 + 透视校正插值
void rasterizeTriangle(Framebuffer& fb, ScreenVert a, ScreenVert b, ScreenVert c, bool backfaceCull) {
    float area = edgeFunction(a.x, a.y, b.x, b.y, c.x, c.y);

    // 屏幕 y 轴朝下会把逆时针翻成顺时针，所以正面 = 负面积
    if (backfaceCull && area > 0.0f) return;
    if (area < 0.0f) {  // 统一成 area > 0：此时三个边函数都 >= 0 即在内部
        std::swap(b, c);
        area = -area;
    }
    if (area < 1e-7f) return;  // 退化三角形

    const int minX = std::max(0, static_cast<int>(std::floor(std::min({a.x, b.x, c.x}))));
    const int maxX = std::min(fb.width - 1, static_cast<int>(std::ceil(std::max({a.x, b.x, c.x}))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({a.y, b.y, c.y}))));
    const int maxY = std::min(fb.height - 1, static_cast<int>(std::ceil(std::max({a.y, b.y, c.y}))));
    if (minX > maxX || minY > maxY) return;

    const float invArea = 1.0f / area;

    for (int y = minY; y <= maxY; ++y) {
        const float py = static_cast<float>(y) + 0.5f;  // 采样点取像素中心
        uint32_t* crow = fb.colorRow(y);
        float* drow = fb.depthRow(y);

        for (int x = minX; x <= maxX; ++x) {
            const float px = static_cast<float>(x) + 0.5f;

            const float w0 = edgeFunction(b.x, b.y, c.x, c.y, px, py);
            if (w0 < 0.0f) continue;
            const float w1 = edgeFunction(c.x, c.y, a.x, a.y, px, py);
            if (w1 < 0.0f) continue;
            const float w2 = edgeFunction(a.x, a.y, b.x, b.y, px, py);
            if (w2 < 0.0f) continue;

            const float l0 = w0 * invArea;
            const float l1 = w1 * invArea;
            const float l2 = w2 * invArea;

            // NDC z 在屏幕空间是仿射的，所以线性插值就是精确的深度
            const float zNdc = l0 * a.z + l1 * b.z + l2 * c.z;
            const float d = zNdc * 0.5f + 0.5f;  // -> [0,1]
            if (d >= drow[x]) continue;          // 深度测试：更近才写入

            // 透视校正：属性按 1/w 加权插值，再除以插值后的 1/w
            const float iw = l0 * a.invW + l1 * b.invW + l2 * c.invW;
            if (iw <= 0.0f) continue;
            const Vec3 col =
                (a.color * (l0 * a.invW) + b.color * (l1 * b.invW) + c.color * (l2 * c.invW)) *
                (1.0f / iw);

            drow[x] = d;
            crow[x] = packColor(col);
        }
    }
}

// ================================================================ 场景

struct Vertex {
    Vec3 pos;
    Vec3 color;
};

struct Triangle {
    int i0 = 0, i1 = 0, i2 = 0;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
};

// 加一个四边形，p0..p3 从正面看逆时针
void addQuad(Mesh& mesh, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 base, bool gradient = true) {
    const int first = static_cast<int>(mesh.vertices.size());

    // 顶点色：以面的基色为主，再按位置做一点渐变，方便看出插值
    auto tint = [&](Vec3 p) {
        if (!gradient) return base;
        const Vec3 k{0.5f + 0.5f * p.x, 0.5f + 0.5f * p.y, 0.5f + 0.5f * p.z};
        return base * 0.85f + k * 0.15f;
    };

    mesh.vertices.push_back({p0, tint(p0)});
    mesh.vertices.push_back({p1, tint(p1)});
    mesh.vertices.push_back({p2, tint(p2)});
    mesh.vertices.push_back({p3, tint(p3)});

    mesh.triangles.push_back({first + 0, first + 1, first + 2});
    mesh.triangles.push_back({first + 0, first + 2, first + 3});
}

// 轴对齐立方体，边长 1，中心在原点
Mesh makeCube() {
    Mesh m;
    const float h = 0.5f;

    addQuad(m, {h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}, {0.85f, 0.35f, 0.30f});        // +X
    addQuad(m, {-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}, {0.30f, 0.55f, 0.85f});    // -X
    addQuad(m, {-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}, {0.90f, 0.80f, 0.40f});        // +Y
    addQuad(m, {-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}, {0.40f, 0.40f, 0.45f});    // -Y
    addQuad(m, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}, {0.55f, 0.80f, 0.55f});        // +Z
    addQuad(m, {h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}, {0.75f, 0.45f, 0.85f});    // -Z
    return m;
}

// 棋盘格地板：铺一片小四边形而不是一整块，顺便验证大量三角形的深度排序
Mesh makeFloor(float y, float halfExtent, float tile) {
    Mesh m;
    const int n = static_cast<int>(std::round(2.0f * halfExtent / tile));
    const Vec3 light{0.26f, 0.28f, 0.34f};
    const Vec3 dark{0.16f, 0.17f, 0.22f};

    for (int iz = 0; iz < n; ++iz) {
        for (int ix = 0; ix < n; ++ix) {
            const float x0 = -halfExtent + ix * tile;
            const float z0 = -halfExtent + iz * tile;
            const float x1 = x0 + tile;
            const float z1 = z0 + tile;
            // 从上方看逆时针：(-,-) -> (+,-) -> (+,+) -> (-,+)
            addQuad(m, {x0, y, z1}, {x1, y, z1}, {x1, y, z0}, {x0, y, z0},
                    ((ix + iz) % 2 == 0) ? light : dark, false);
        }
    }
    return m;
}

struct Object {
    Mesh mesh;
    Vec3 position;
    float scale = 1.0f;
    float spinSpeed = 1.0f;  // 弧度/秒
    float tilt = 0.35f;      // 固定的绕 X 轴倾角
    float phase = 0.0f;      // 起始相位
};

struct Camera {
    Vec3 eye{2.6f, 1.55f, 3.5f};
    Vec3 target{0.0f, -0.15f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    float fovY = 45.0f * 3.14159265358979323846f / 180.0f;
    float zNear = 0.1f;
    float zFar = 100.0f;
};

struct RenderOptions {
    bool backfaceCull = true;
    bool wireframe = false;
    int ssaa = 2;                                           // 超采样倍数，1 = 关闭
    Vec3 lightDir = normalize(Vec3(-0.35f, 0.75f, 0.55f));  // 指向光源的方向
    Vec3 fogColor{0.075f, 0.085f, 0.115f};                  // 与背景地平线附近接近
};

// 模型空间 -> 屏幕空间；任一点跑到相机后面就返回 false（没有做近平面裁剪）
bool projectTriangle(Framebuffer& fb, const Mat4& mvp, const Vec3 pos[3], const Vec3 col[3],
                     ScreenVert out[3]) {
    Vec4 clip[3];
    for (int i = 0; i < 3; ++i) {
        clip[i] = mvp * Vec4(pos[i], 1.0f);
        if (clip[i].w <= 1e-5f) return false;
    }

    for (int i = 0; i < 3; ++i) {
        const float invW = 1.0f / clip[i].w;
        const float ndcX = clip[i].x * invW;
        const float ndcY = clip[i].y * invW;
        const float ndcZ = clip[i].z * invW;

        out[i].x = (ndcX * 0.5f + 0.5f) * static_cast<float>(fb.width);
        out[i].y = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(fb.height);
        out[i].z = ndcZ;
        out[i].invW = invW;
        out[i].color = col[i];
    }
    return true;
}

void renderScene(Framebuffer& fb, float time, const Camera& cam, const std::vector<Object>& objects,
                 const RenderOptions& opt) {
    // 背景：竖向渐变
    const Vec3 top{0.12f, 0.14f, 0.20f};
    const Vec3 bottom{0.02f, 0.02f, 0.03f};
    for (int y = 0; y < fb.height; ++y) {
        const float t = fb.height > 1 ? static_cast<float>(y) / static_cast<float>(fb.height - 1) : 0.0f;
        std::fill(fb.colorRow(y), fb.colorRow(y) + fb.width, packColor(mix(top, bottom, t)));
    }
    fb.clearDepth();

    const float aspect = static_cast<float>(fb.width) / static_cast<float>(fb.height);
    const Mat4 view = lookAt(cam.eye, cam.target, cam.up);
    const Mat4 proj = perspective(cam.fovY, aspect, cam.zNear, cam.zFar);
    const Mat4 viewProj = proj * view;

    const uint32_t wireColor = packColor({0.85f, 0.88f, 0.95f});

    for (const Object& obj : objects) {
        const Mat4 model = translation(obj.position) * rotationY(time * obj.spinSpeed + obj.phase) *
                           rotationX(obj.tilt) * scaling(obj.scale);
        const Mat4 mvp = viewProj * model;

        for (const Triangle& tri : obj.mesh.triangles) {
            const Vertex& v0 = obj.mesh.vertices[size_t(tri.i0)];
            const Vertex& v1 = obj.mesh.vertices[size_t(tri.i1)];
            const Vertex& v2 = obj.mesh.vertices[size_t(tri.i2)];

            const Vec3 wp0 = transformPoint(model, v0.pos);
            const Vec3 wp1 = transformPoint(model, v1.pos);
            const Vec3 wp2 = transformPoint(model, v2.pos);

            // 面法线 + Lambert 主光 + 朝向相机的补光（避免背光面纯黑）
            const Vec3 normal = normalize(cross(wp1 - wp0, wp2 - wp0));
            const float ndl = std::max(0.0f, dot(normal, opt.lightDir));
            const Vec3 faceCenter = (wp0 + wp1 + wp2) * (1.0f / 3.0f);
            const float fill = std::max(0.0f, dot(normal, normalize(cam.eye - faceCenter)));
            const float shade = 0.28f + 0.72f * ndl + 0.22f * fill;

            // 距离雾：远处向背景色靠拢，顺便把地板的“硬边”藏掉，也强化纵深
            const float dist = length(faceCenter - cam.eye);
            const float fog = std::min(0.85f, std::max(0.0f, (dist - 5.0f) / 9.0f));

            const Vec3 cols[3] = {mix(v0.color * shade, opt.fogColor, fog),
                                  mix(v1.color * shade, opt.fogColor, fog),
                                  mix(v2.color * shade, opt.fogColor, fog)};
            const Vec3 pos[3] = {v0.pos, v1.pos, v2.pos};

            ScreenVert sv[3];
            if (!projectTriangle(fb, mvp, pos, cols, sv)) continue;

            if (opt.wireframe) {
                for (int i = 0; i < 3; ++i) {
                    const ScreenVert& p = sv[i];
                    const ScreenVert& q = sv[(i + 1) % 3];
                    drawLine(fb, static_cast<int>(p.x), static_cast<int>(p.y), static_cast<int>(q.x),
                             static_cast<int>(q.y), wireColor);
                }
            } else {
                rasterizeTriangle(fb, sv[0], sv[1], sv[2], opt.backfaceCull);
            }
        }
    }
}

// ================================================================ 输出

// 带超采样的整帧渲染：先按 ssaa 倍分辨率光栅化，再盒式降采样到 target
void renderFrame(Framebuffer& target, Framebuffer& scratch, float time, const Camera& cam,
                 const std::vector<Object>& objects, const RenderOptions& opt) {
    if (opt.ssaa <= 1) {
        renderScene(target, time, cam, objects, opt);
        return;
    }
    if (scratch.width != target.width * opt.ssaa || scratch.height != target.height * opt.ssaa) {
        renderScene(target, time, cam, objects, opt);  // 尺寸没准备好就退化成不抗锯齿
        return;
    }
    renderScene(scratch, time, cam, objects, opt);
    downsampleBox(scratch, target);
}

bool writePPM(const std::string& path, const Framebuffer& fb) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", fb.width, fb.height);

    std::vector<unsigned char> row(size_t(fb.width) * 3);
    for (int y = 0; y < fb.height; ++y) {
        for (int x = 0; x < fb.width; ++x) {
            const uint32_t c = fb.color[size_t(y) * size_t(fb.width) + size_t(x)];
            row[size_t(x) * 3 + 0] = static_cast<unsigned char>((c >> 16) & 0xFFu);
            row[size_t(x) * 3 + 1] = static_cast<unsigned char>((c >> 8) & 0xFFu);
            row[size_t(x) * 3 + 2] = static_cast<unsigned char>(c & 0xFFu);
        }
        std::fwrite(row.data(), 1, row.size(), f);
    }
    std::fclose(f);
    return true;
}

std::vector<Object> makeScene() {
    std::vector<Object> objects;

    const float floorY = -1.0f;

    Object floor;
    floor.mesh = makeFloor(floorY, 6.0f, 1.0f);
    floor.position = {0.0f, 0.0f, 0.0f};
    floor.spinSpeed = 0.0f;  // 地板不转
    floor.tilt = 0.0f;
    objects.push_back(floor);

    // 立方体都“坐”在地板上：中心高度 = 地板 + 半个边长。
    // 不绕 X 轴倾斜，否则旋转时角会插进地板；再抬高一点避免底面与地板共面导致 z-fighting
    const float eps = 0.003f;

    Object mainCube;
    mainCube.mesh = makeCube();
    mainCube.scale = 1.1f;
    mainCube.position = {-0.95f, floorY + 0.5f * 1.1f + eps, 0.15f};
    mainCube.spinSpeed = 0.9f;
    mainCube.tilt = 0.0f;
    mainCube.phase = 0.35f;
    objects.push_back(mainCube);

    Object backCube;
    backCube.mesh = makeCube();
    backCube.scale = 0.75f;
    backCube.position = {1.05f, floorY + 0.5f * 0.75f + eps, -1.35f};
    backCube.spinSpeed = -1.4f;
    backCube.tilt = 0.0f;
    backCube.phase = 0.7f;
    objects.push_back(backCube);

    return objects;
}

int main(int argc, char** argv) {
    std::string dumpPath;
    float dumpTime = 0.0f;
    int width = 960;
    int height = 540;
    bool wireframe = false;
    bool cull = true;
    int benchFrames = 0;
    int ssaa = 2;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--dump" && i + 1 < argc) {
            dumpPath = argv[++i];
        } else if (arg == "--time" && i + 1 < argc) {
            dumpTime = std::stof(argv[++i]);
        } else if (arg == "--size" && i + 2 < argc) {
            width = std::atoi(argv[++i]);
            height = std::atoi(argv[++i]);
        } else if (arg == "--ssaa" && i + 1 < argc) {
            ssaa = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--wireframe") {
            wireframe = true;
        } else if (arg == "--no-cull") {
            cull = false;
        } else if (arg == "--bench" && i + 1 < argc) {
            benchFrames = std::atoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::printf("用法: soft_raster [--dump 文件.ppm] [--time 秒] [--size 宽 高] [--ssaa 倍数]\n"
                        "                  [--wireframe] [--no-cull] [--bench 帧数]\n");
            return 0;
        }
    }

    if (width <= 0 || height <= 0) {
        std::fprintf(stderr, "尺寸不合法: %d x %d\n", width, height);
        return 1;
    }

    const std::vector<Object> objects = makeScene();
    const Camera camera;

    RenderOptions opt;
    opt.wireframe = wireframe;
    opt.backfaceCull = cull;
    opt.ssaa = ssaa;

    // ---------------- 基准测试：离屏连续渲染，报平均耗时 ----------------
    if (benchFrames > 0) {
        Framebuffer fb(width, height);
        Framebuffer scratch(width * ssaa, height * ssaa);
        // 先跑几帧预热，避免把首次分配算进去
        for (int i = 0; i < 5; ++i) renderFrame(fb, scratch, 0.0f, camera, objects, opt);

        const uint64_t begin = SDL_GetPerformanceCounter();
        for (int i = 0; i < benchFrames; ++i)
            renderFrame(fb, scratch, static_cast<float>(i) * 0.016f, camera, objects, opt);
        const uint64_t end = SDL_GetPerformanceCounter();

        const double seconds = static_cast<double>(end - begin) / static_cast<double>(SDL_GetPerformanceFrequency());
        const double perFrame = seconds / benchFrames;
        std::printf("%d 帧，平均 %.3f ms/帧，约 %.0f FPS（%dx%d, ssaa=%d）\n", benchFrames, perFrame * 1000.0,
                    1.0 / perFrame, width, height, ssaa);
        return 0;
    }

    // ---------------- 离屏模式：渲染一帧存成 PPM ----------------
    if (!dumpPath.empty()) {
        Framebuffer fb(width, height);
        Framebuffer scratch(width * ssaa, height * ssaa);
        renderFrame(fb, scratch, dumpTime, camera, objects, opt);
        if (!writePPM(dumpPath, fb)) {
            std::fprintf(stderr, "写文件失败: %s\n", dumpPath.c_str());
            return 1;
        }
        std::printf("已输出 %s (%dx%d)\n", dumpPath.c_str(), width, height);
        return 0;
    }

    // ---------------- 窗口模式 ----------------
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init 失败: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("soft raster", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
    if (!window) {
        std::fprintf(stderr, "创建窗口失败: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        std::fprintf(stderr, "创建渲染器失败: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!texture) {
        std::fprintf(stderr, "创建纹理失败: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Framebuffer fb(width, height);
    Framebuffer scratch(width * ssaa, height * ssaa);

    std::printf("操作: SPACE 线框  C 背面剔除  A 抗锯齿  R 重置时间  ESC 退出\n");

    bool running = true;
    float time = 0.0f;
    uint64_t lastCounter = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());

    uint64_t fpsTimer = lastCounter;
    int frames = 0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_SPACE: opt.wireframe = !opt.wireframe; break;
                    case SDLK_c: opt.backfaceCull = !opt.backfaceCull; break;
                    case SDLK_a: opt.ssaa = (opt.ssaa > 1) ? 1 : 2; break;
                    case SDLK_r: time = 0.0f; break;
                    default: break;
                }
            }
        }

        const uint64_t now = SDL_GetPerformanceCounter();
        const float dt = static_cast<float>(static_cast<double>(now - lastCounter) / freq);
        lastCounter = now;
        time += dt;

        renderFrame(fb, scratch, time, camera, objects, opt);

        SDL_UpdateTexture(texture, nullptr, fb.color.data(), width * static_cast<int>(sizeof(uint32_t)));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        // 每 0.5 秒更新一次标题里的 FPS
        ++frames;
        if (static_cast<double>(now - fpsTimer) / freq >= 0.5) {
            const double seconds = static_cast<double>(now - fpsTimer) / freq;
            char title[128];
            std::snprintf(title, sizeof(title), "soft raster  |  %.1f FPS  |  %s%s%s", frames / seconds,
                          opt.wireframe ? "wireframe " : "", opt.backfaceCull ? "cull " : "",
                          opt.ssaa > 1 ? "ssaa" : "");
            SDL_SetWindowTitle(window, title);
            frames = 0;
            fpsTimer = now;
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
