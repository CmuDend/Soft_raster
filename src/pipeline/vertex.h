#pragma once

#include "math/vec.h"

// 输入顶点（模型空间）。D3 Part 3 起带法线，光照要用。
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
};

// 顶点着色器输出：裁剪空间坐标 + 后续片元要用的世界空间属性（指导 §5.2）。
// 注意这里还**没有**做透视除法，w 要留给视口变换（1/w 是透视校正的钥匙）。
struct VSOut {
    Vec4 clip;
    Vec3 worldPos;
    Vec3 normal;   // 世界空间，已经过法线矩阵
    Vec2 uv;
};

// 片元着色器的输入：光栅化器把 VSOut 的属性透视校正插值后传进来。
struct Fragment {
    Vec3 worldPos;
    Vec3 normal;   // 插值后的法线长度不再是 1，着色器里要重新 normalize
    Vec2 uv;
};
