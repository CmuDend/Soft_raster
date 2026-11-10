#pragma once

#include <cmath>

struct Vec2 { float x = 0, y = 0; };
struct Vec3 { float x = 0, y = 0, z = 0; };
struct Vec4 { float x = 0, y = 0, z = 0, w = 0; };

// ---------- 运算符：给你一个完整示范，其余照这个模式写 ----------
inline Vec3 operator+(Vec3 a, Vec3 b) { 
    return {a.x + b.x, a.y + b.y, a.z + b.z}; 
}

inline Vec3 operator-(Vec3 a, Vec3 b) {
    // TODO：照 operator+ 的模式，改成减法
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 operator*(Vec3 a, float s) {
    // TODO：照 operator+ 的模式，三个分量都乘 s
    return {a.x * s, a.y * s, a.z * s};
}

inline Vec3 operator*(float s, Vec3 a) { return a * s; }  // 直接复用上面的

inline Vec3& operator+=(Vec3& a, Vec3 b) {
    // TODO：a 的三个分量各自加上 b，然后 return a;
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

// ---------- 点积 / 叉积 / 长度 / 归一化 ----------
inline float dot(Vec3 a, Vec3 b) {
    // TODO：a.x*b.x + a.y*b.y + a.z*b.z
    return a.x * b.x + a.y * b.y +a.z * b.z;
    
}

inline Vec3 cross(Vec3 a, Vec3 b) {
    // TODO：三个分量公式见指导 §6 相关推导，或任何线性代数书
    return { a.y * b.z - a.z * b.y,
             a.z * b.x - a.x * b.z,
             a.x * b.y - a.y * b.x};
}

inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }  // 依赖 dot，先把 dot 写完

inline Vec3 normalize(Vec3 a) {
    // TODO：a / length(a)；但 length≈0 时要返回 {}，否则除以 0 得到 NaN
    float len = length(a);
    if(len<1e-8f){
        return{};
    }
    return a * (1.0f / len);
}
