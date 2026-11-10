#pragma once

#include <cmath>

#include "math/vec.h"

// 4x4 矩阵，列主序存储：m[col * 4 + row]，和 OpenGL 一致（指导 §6.1）。
// 这是 header-only：函数体必须写在这里（用 inline），不配 .cpp。
struct Mat4 {
    float m[16] = {};

    static Mat4 identity() {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }
    static Mat4 zero() { return Mat4{}; }

    float&       operator()(int r, int c)       { return m[c * 4 + r]; }
    const float& operator()(int r, int c) const { return m[c * 4 + r]; }
};

// a * b：先作用 b（列向量约定 v' = M * v）
inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;  // m[16] = {} 默认清零，所以下面可以用 +=
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr)
            for (int k = 0; k < 4; ++k)
                r(rr, c) += a(rr, k) * b(k, c);
    return r;
}

inline Vec4 operator*(const Mat4& a, const Vec4& v) {
    // TODO：见指导 §6.1 的 inline Vec4 operator*
    return {
        a.m[0]*v.x + a.m[4]*v.y + a.m[8] *v.z + a.m[12]*v.w,
        a.m[1]*v.x + a.m[5]*v.y + a.m[9] *v.z + a.m[13]*v.w,
        a.m[2]*v.x + a.m[6]*v.y + a.m[10]*v.z + a.m[14]*v.w,
        a.m[3]*v.x + a.m[7]*v.y + a.m[11]*v.z + a.m[15]*v.w,
    };
}

inline Mat4 perspective(float fovYRad, float aspect, float n, float f) {
    // TODO：直接抄指导 §6.2，逐项填到 m 里（注意列主序下标）
   
        float t = 1.0f / std::tan(fovYRad * 0.5f);
    Mat4 m = Mat4::zero();
    m(0,0) = t / aspect;
    m(1,1) = t;
    m(2,2) = (f + n) / (n - f);
    m(2,3) = (2.0f * f * n) / (n - f);
    m(3,2) = -1.0f;                 // w_clip = -z_view > 0
    return m;
    
}

inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    // TODO：直接抄指导 §6.3（f/s/u 用上面的 normalize/cross/dot）
    
    
        Vec3 f = normalize(center - eye);
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);
    Mat4 m = Mat4::identity();
    m(0,0)= s.x;  m(0,1)= s.y;  m(0,2)= s.z;  m(0,3)= -dot(s, eye);
    m(1,0)= u.x;  m(1,1)= u.y;  m(1,2)= u.z;  m(1,3)= -dot(u, eye);
    m(2,0)=-f.x;  m(2,1)=-f.y;  m(2,2)=-f.z;  m(2,3)=  dot(f, eye);
    return m;
    
}

inline Mat4 translation(Vec3 t) {
    // TODO：单位矩阵，把 t 放到第 3 列（下标 (0,3)(1,3)(2,3)）
    
        Mat4 m = Mat4::identity();
        m(0,3) = t.x;
        m(1,3) = t.y;
        m(2,3) = t.z;
        return m;
    
}

inline Mat4 scaling(float s) {
    // TODO：对角线上前三个元素 = s
    
        Mat4 m = Mat4::identity();
        m(0,0) = s;
        m(1,1) = s;
        m(2,2) = s;
        return m;
    
}

inline Mat4 rotationX(float rad) {
    // TODO：绕 X 轴，只动第 1、2 行/列
    
        Mat4 m = Mat4::identity();
        float c = std::cos(rad);
        float s = std::sin(rad);
        m(1,1) = c;  m(1,2) = -s;
        m(2,1) = s;  m(2,2) = c;
        return m;
    
}

inline Mat4 rotationY(float rad) {
    // TODO：绕 Y 轴，只动第 0、2 行/列
   
        Mat4 m = Mat4::identity();
        float c = std::cos(rad);
        float s = std::sin(rad);
        m(0,0) = c;  m(0,2) = s;
        m(2,0) = -s;  m(2,2) = c;
        return m;
   
}
