#include "pipeline/renderer.h"

#include <chrono>
#include <thread>

#include "pipeline/clip.h"
#include "pipeline/tiles.h"

namespace {

using Clock = std::chrono::steady_clock;

double msSince(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// 帧间复用，避免每帧重新分配 ~1MB
std::vector<ScreenTri>& scratch() {
    static thread_local std::vector<ScreenTri> v;
    return v;
}

}  // namespace

void renderScene(Framebuffer& fb, const std::vector<DrawCall>& calls, const Mat4& viewProj,
                 const RenderOptions& opt, RenderStats* stats) {
    RenderStats local;
    local.width = fb.width();
    local.height = fb.height();

    std::vector<ScreenTri>& tris = scratch();
    tris.clear();

    // ---------- 阶段 1：顶点变换 + 近平面裁剪（指导 §10.4 的阶段 1）----------
    const Clock::time_point t0 = Clock::now();
    for (const DrawCall& call : calls) {
        if (!call.mesh || !call.shader) continue;

        const Mesh& mesh = *call.mesh;
        const Mat4 mvp = viewProj * call.model;
        const Mat3 nrmMat = normalMatrix(call.model);

        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            ++local.triangles;

            VSOut tri[3];
            for (int k = 0; k < 3; ++k) {
                const Vertex& v = mesh.verts[size_t(mesh.indices[i + size_t(k)])];
                const Vec4 pos{v.position.x, v.position.y, v.position.z, 1.0f};
                const Vec4 wpos = call.model * pos;

                VSOut& o = tri[k];
                o.clip = mvp * pos;
                o.worldPos = {wpos.x, wpos.y, wpos.z};
                o.normal = normalize(nrmMat * v.normal);
                o.uv = v.uv;
            }

            VSOut poly[4];
            int n = 0;
            if (opt.clipNear) {
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
            if (n < 3) {
                ++local.culled;
                continue;
            }

            SVert s[4];
            for (int k = 0; k < n; ++k) s[k] = toScreen(poly[k], fb.width(), fb.height());

            // 扇形三角化：n == 3 就是一个三角形，n == 4 切成两个
            for (int k = 0; k + 2 < n; ++k) {
                tris.push_back({s[0], s[k + 1], s[k + 2], call.shader});
                ++local.emitted;
            }
        }
    }
    const Clock::time_point t1 = Clock::now();

    // ---------- 阶段 2：分箱（指导 §10.4 的阶段 2）----------
    const TileLists tiles = binTriangles(tris, fb.width(), fb.height(), opt.tileSize);
    const Clock::time_point t2 = Clock::now();

    // ---------- 阶段 3：逐 tile 光栅化（指导 §10.4 的阶段 3）----------
    const int threads = opt.threads > 0
                            ? opt.threads
                            : std::max(1, int(std::thread::hardware_concurrency()));
    RasterOptions ropt;
    ropt.cullBack = opt.cullBack;
    ropt.perspectiveCorrect = opt.perspectiveCorrect;
    ropt.incrementalEdges = opt.incrementalEdges;

    RasterCounters counters;
    renderTiles(fb, tris, tiles, ropt, threads, &counters);
    const Clock::time_point t3 = Clock::now();

    local.msVertex = msSince(t0, t1);
    local.msBin = msSince(t1, t2);
    local.msRaster = msSince(t2, t3);
    local.tiles = tiles.tileCount();
    local.threads = threads;
    local.covered = counters.covered;
    local.shaded = counters.shaded;

    if (stats) *stats = local;
}
