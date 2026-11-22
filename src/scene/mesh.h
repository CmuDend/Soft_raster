#pragma once

#include <vector>

#include "pipeline/vertex.h"

// 一个可渲染的网格：顶点数组 + 三角形索引（每 3 个下标一个三角形）。
// 放在 scene/ 而不是 pipeline/：管线不该知道"模型从哪来"。
struct Mesh {
    std::vector<Vertex> verts;
    std::vector<int> indices;

    size_t triangleCount() const { return indices.size() / 3; }
};

// 程序化基础体（没有 OBJ 时的兜底，也是 D4 加载失败时的降级方案）
Mesh makeCube();
// 地面用网格而不是一整块四边形：整块的远端角点一旦跑到相机背后，
// 没做裁剪的管线会整片丢弃（D5 才修）。切成小块后丢的只是看不见的那几块。
Mesh makeFloor(float y, float half, int cells);
Mesh makeSphere(float radius, int rings, int segs);
