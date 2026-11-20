#include "pipeline/shader.h"

#include <algorithm>
#include <cmath>

namespace {

// 0xAARRGGBB → [0,1] 的 RGB。注意：真正的引擎这里还要做 sRGB → 线性。
Vec3 toVec3(Color c) {
    return {float((c >> 16) & 0xFFu) / 255.0f,
            float((c >> 8) & 0xFFu) / 255.0f,
            float(c & 0xFFu) / 255.0f};
}

Color toColor(Vec3 c) {
    auto q = [](float v) { return uint8_t(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    return rgb(q(c.x), q(c.y), q(c.z));
}

}  // namespace

Color PhongShader::fragment(const Fragment& f) const {
    // 1) 法线：重心插值后的长度不再是 1，必须重新归一化（否则亮度全错）
    const Vec3 N = normalizeNormal ? normalize(f.normal) : f.normal;

    // 2) V：从表面指向相机的单位向量
    const Vec3 V = normalize(m_cameraPos - f.worldPos);

    // 3) 底色：有贴图就双线性采样，没有就用固有色
    const Vec3 base = m_tex ? toVec3(m_tex->sample(f.uv)) : albedo;

    // 4) 环境光：防止背光面纯黑
    Vec3 color = base * ambient;

    // 5) 逐个光源：漫反射 + 高光
    const Vec3 l = normalize(light.position - f.worldPos);
    const float ndl = std::max(dot(N, l), 0.0f);   // Lambert：光线越正对表面越亮

    float spec = 0.0f;
    if (useSpecular) {
        // 半程向量：l 和 V 的角平分线；N 越接近它，高光越强
        const Vec3 H = normalize(l + V);
        spec = std::pow(std::max(dot(N, H), 0.0f), shininess) * specStrength;
    }

    const Vec3 lc = light.color * light.intensity;   // 光源颜色 × 强度
    // 漫反射乘底色（有色表面），高光是白点（非金属的高光不带底色）
    color.x += (base.x * ndl + spec) * lc.x;
    color.y += (base.y * ndl + spec) * lc.y;
    color.z += (base.z * ndl + spec) * lc.z;

    return toColor(color);
}
