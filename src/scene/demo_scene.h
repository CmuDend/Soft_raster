#pragma once

#include <string>
#include <vector>

#include "core/texture.h"
#include "pipeline/renderer.h"
#include "pipeline/shader.h"
#include "scene/camera.h"
#include "scene/mesh.h"

// 对照实验开关（README 的"踩坑图"就靠它们生成）
struct SceneOptions {
    bool specular = true;         // 关掉 → 只剩 Lambert
    bool normalizeNormal = true;  // 关掉 → 法线没归一化
};

// D4 起用的演示场景：棋盘地面 + 圆环（OBJ）+ 蓝色球。
// 把场景从 main.cpp 抽出来，是为了让 bench 和窗口跑的是同一份东西。
class DemoScene {
public:
    DemoScene();

    // 加载失败会返回 false 并回退到程序化立方体（指导 §13 D4 的降级方案）
    bool loadModel(const std::string& path, std::string* error = nullptr);

    void setOptions(const SceneOptions& opt) { m_opt = opt; }
    const SceneOptions& options() const { return m_opt; }

    // 每帧调用：更新材质里的相机位置、模型矩阵，重建 draw call 列表
    void update(float t, const OrbitCamera& cam);

    const std::vector<DrawCall>& calls() const { return m_calls; }
    size_t triangleCount() const;
    const Mesh& model() const { return m_torus; }

private:
    Texture m_checker;
    Mesh m_floor;
    Mesh m_torus;
    Mesh m_sphere;

    PhongShader m_floorMat;
    PhongShader m_torusMat;
    PhongShader m_solidMat;

    std::vector<DrawCall> m_calls;
    SceneOptions m_opt;
};
