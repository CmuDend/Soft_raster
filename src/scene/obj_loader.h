#pragma once

#include <string>

#include "scene/mesh.h"

// 读 Wavefront OBJ（只关心 v / vt / vn / f）：
//   - f 支持 v、v/vt、v//vn、v/vt/vn 四种写法；索引可为负（相对文件末尾）
//   - 多边形按扇形三角化（凸多边形足够）
//   - 没有 vn 的文件：按面法线重新生成，每个三角形独立顶点（不做平滑）
// 失败时返回 false，并把原因写进 error（可为 nullptr）。
bool loadOBJ(const std::string& path, Mesh& out, std::string* error = nullptr);
