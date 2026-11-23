#pragma once

#include <vector>

#include "core/framebuffer.h"
#include "pipeline/rasterizer.h"

// D7：tile 分箱 + 并行光栅化（指导 §10.4）
//
// 思路是"二维分桶"：把屏幕切成 tileSize × tileSize 的格子，每个三角形先算屏幕包围盒，
// 再把自己登记到所有与包围盒相交的 tile 名下。之后每个 tile 独立渲染，只写自己那块像素。
//
// 为什么不用锁：每个像素恰好属于一个 tile，深度值和颜色都只有该 tile 的线程会碰，
// 写集合互不相交。
// 为什么没有假共享：tile 宽 32 px × 4 B = 128 B，正好是两条 cache line 的整数倍，
// 相邻 tile 不会落在同一条 cache line 上。

// 屏幕空间三角形 + 它该用哪个着色器（renderer 的生产者、tiler 的消费者共用）
struct ScreenTri {
    SVert a, b, c;
    const IShader* shader = nullptr;
};

// 一个 tile 的三角形列表，CSR 压缩存储：
// tile t 的三角形是 indices[offsets[t] .. offsets[t+1])
struct TileLists {
    int tileSize = 32;
    int cols = 0;
    int rows = 0;
    std::vector<int> offsets;  // size = cols*rows + 1
    std::vector<int> indices;  // 三角形下标，按 tile 分组

    int tileCount() const { return cols * rows; }
    int tileX0(int t) const { return (t % cols) * tileSize; }
    int tileY0(int t) const { return (t / cols) * tileSize; }
};

// 阶段 2：分箱。单线程即可——6.5k 三角形分箱只有几十微秒，
// 为它开线程池的调度开销比干活的还大（bench 里 msBinning 就是这个数）。
TileLists binTriangles(const std::vector<ScreenTri>& tris, int width, int height, int tileSize);

// 阶段 3：多线程逐 tile 渲染。threads <= 1 时退化成单线程顺序执行（用于对照）。
// covered/shaded 由每个线程本地累加、最后归并，避免原子操作污染热点路径。
void renderTiles(Framebuffer& fb, const std::vector<ScreenTri>& tris, const TileLists& tiles,
                 const RasterOptions& baseOpt, int threads, RasterCounters* counters);
