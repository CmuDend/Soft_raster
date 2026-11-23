#pragma once

#include "pipeline/vertex.h"

// ===== D5：齐次空间近平面裁剪（Sutherland–Hodgman 单平面版，指导 §7.3）=====
//
// 为什么必须在裁剪空间做、不能用 NDC：
//   跨近平面的三角形有一个顶点 w ≤ 0，做透视除法会把它的坐标"翻"到反向，
//   屏幕上出现一条从三角形炸到全屏的细长条。而且属性插值只有齐次空间里才是线性的。

// 近平面有符号距离（OpenGL 约定，NDC z ∈ [-1,1]）：z_clip + w_clip > 0 表示在内侧
inline float nearDist(const VSOut& v) { return v.clip.z + v.clip.w; }

// 齐次空间线性插值：clip 与所有属性都按同一个 t 插，插完不需要再做透视除法
VSOut lerpClip(const VSOut& a, const VSOut& b, float t);

// 把三角形裁到近平面以内。out 至少要有 4 个位置，返回写入了几个顶点：
//   3 个全在内 → 3；1 个在外 → 4（切成两个三角形）；2 个在外 → 3；3 个全在外 → 0
int clipTriangleNear(const VSOut in[3], VSOut out[4]);
