#include "scene/demo_scene.h"

#include "scene/obj_loader.h"

namespace {

// 棋盘格纹理：size×size 个纹素，每格 cell×cell 个纹素
Texture makeCheckerboard(int size, int cell) {
    Texture t(size, size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const int cx = x / cell, cy = y / cell;
            t.setTexel(x, y, ((cx + cy) & 1) ? rgb(122, 52, 40) : rgb(222, 210, 186));
        }
    t.setWrap(Wrap::Repeat);  // 地面 uv 会超出 [0,1]，要平铺
    return t;
}

Light demoLight() {
    Light l;
    l.position = {3.5f, 6.0f, 4.0f};
    l.intensity = 1.0f;
    return l;
}

}  // namespace

DemoScene::DemoScene()
    : m_checker(makeCheckerboard(64, 8)),
      m_floor(makeFloor(-1.35f, 20.0f, 32)),
      m_torus(makeCube()),
      m_sphere(makeSphere(0.62f, 24, 32)),
      m_floorMat(&m_checker),
      m_torusMat(&m_checker),
      m_solidMat(nullptr) {
    // 地面：贴图 + 弱高光（太强会一片惨白）
    m_floorMat.light = demoLight();
    m_floorMat.specStrength = 0.18f;
    m_floorMat.shininess = 32.0f;

    // 圆环：贴图 + 强高光
    m_torusMat.light = demoLight();
    m_torusMat.specStrength = 0.9f;
    m_torusMat.shininess = 48.0f;

    // 纯色材质：球体（高光最清楚）
    m_solidMat.albedo = {0.16f, 0.34f, 0.72f};
    m_solidMat.light = demoLight();
    m_solidMat.shininess = 64.0f;
    m_solidMat.specStrength = 0.9f;
}

bool DemoScene::loadModel(const std::string& path, std::string* error) {
    Mesh mesh;
    if (!loadOBJ(path, mesh, error)) {
        m_torus = makeCube();
        return false;
    }
    m_torus = std::move(mesh);
    return true;
}

void DemoScene::update(float t, const OrbitCamera& cam) {
    const Vec3 eye = cam.eye();
    for (PhongShader* s : {&m_floorMat, &m_torusMat, &m_solidMat}) {
        s->setCameraPos(eye);
        s->useSpecular = m_opt.specular;
        s->normalizeNormal = m_opt.normalizeNormal;
    }

    // 圆环：先绕 X 倾斜，再绕 Y 转（右乘先作用），浮在地面上方
    const Mat4 torusModel =
        translation({0.0f, -0.5f, 0.0f}) * rotationY(0.6f * t) * rotationX(0.42f);

    m_calls.clear();
    m_calls.push_back({&m_floor, Mat4::identity(), &m_floorMat});
    m_calls.push_back({&m_torus, torusModel, &m_torusMat});
    m_calls.push_back({&m_sphere, translation({2.05f, -0.73f, 0.85f}), &m_solidMat});
}

size_t DemoScene::triangleCount() const {
    return m_floor.triangleCount() + m_torus.triangleCount() + m_sphere.triangleCount();
}
