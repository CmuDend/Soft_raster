#pragma once

#include <vector>

#include "core/framebuffer.h"
#include "math/mat.h"
#include "pipeline/rasterizer.h"
#include "scene/mesh.h"

// 一次绘制：网格 + 模型矩阵 + 材质。ref 语义，渲染期间调用方保证它们活着。
struct DrawCall {
    const Mesh* mesh = nullptr;
    Mat4 model = Mat4::identity();
    const IShader* shader = nullptr;
};

// 一帧的开关。bench 每次只改一个，保证"优化前后"是可比的。
struct RenderOptions {
    bool clipNear = true;
    bool cullBack = true;
    bool perspectiveCorrect = true;
    bool incrementalEdges = true;
};

// 一帧的统计。D6 之后所有性能结论都必须能追到这里的某个数字。
struct RenderStats {
    size_t triangles = 0;   // 提交的三角形
    size_t culled = 0;      // 被剔除的（全在近平面外 / 退化 / 背面）
    size_t emitted = 0;     // 裁剪后真正进光栅化器的三角形（四顶点会切成两个）
    unsigned long long covered = 0;  // 通过覆盖测试的片元
    unsigned long long shaded = 0;   // 通过深度测试、真正着色了的片元

    double msVertex = 0.0;  // 顶点变换 + 裁剪 + 视口变换
    double msRaster = 0.0;  // 光栅化 + 深度测试 + 着色

    int width = 0;
    int height = 0;

    double msTotal() const { return msVertex + msRaster; }

    // 平均每个像素被覆盖几次：> 1 说明有像素被重复填充（overdraw）
    double overdraw() const {
        const double px = double(width) * double(height);
        return px > 0.0 ? double(covered) / px : 0.0;
    }
    // 通过深度测试的比例：early-Z 挡掉了多少"白跑的片元"
    double shadeRatio() const {
        return covered ? double(shaded) / double(covered) : 0.0;
    }
};

// 两阶段渲染：先把所有三角形变换+裁剪成屏幕空间三角形（CPU 单线程），
// 再统一光栅化。这个分批结构就是 D7 多线程 tile 渲染的接缝处。
void renderScene(Framebuffer& fb, const std::vector<DrawCall>& calls, const Mat4& viewProj,
                 const RenderOptions& opt, RenderStats* stats = nullptr);
