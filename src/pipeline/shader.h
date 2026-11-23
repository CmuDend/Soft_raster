#pragma once

#include "core/framebuffer.h"
#include "core/texture.h"
#include "math/vec.h"
#include "pipeline/rasterizer.h"
#include "pipeline/vertex.h"

// 点光源（D3 只一个；后面做多光源时改成数组）
struct Light {
    Vec3 position{3.0f, 4.0f, 2.0f};
    Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
};

// Blinn-Phong：ambient + diffuse（Lambert）+ specular（半程向量 H）
class PhongShader : public IShader {
public:
    PhongShader(const Texture* tex = nullptr, Vec3 cameraPos = {})
        : m_tex(tex), m_cameraPos(cameraPos) {}

    Color fragment(const Fragment& f) const override;

    void setCameraPos(Vec3 p) { m_cameraPos = p; }
    const Vec3& cameraPos() const { return m_cameraPos; }

    Light light;
    Vec3 albedo{0.85f, 0.85f, 0.85f};  // 没有贴图时的固有色
    float ambient = 0.15f;              // 环境光比例
    float shininess = 48.0f;            // 高光指数：越大越"聚"，越像光滑塑料
    float specStrength = 0.8f;          // 高光强度

    // 对照实验开关（README 的"踩坑图"就靠它们生成）
    bool normalizeNormal = true;        // 关掉 → 法线没归一化，亮度整体变暗且跳变
    bool useSpecular = true;            // 关掉 → 只剩 Lambert（D3 最低验收）

private:
    const Texture* m_tex = nullptr;
    Vec3 m_cameraPos;
};
