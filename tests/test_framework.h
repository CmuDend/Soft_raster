#pragma once

#include <cmath>
#include <cstdio>

// 极简断言框架：不引第三方库，一个可执行文件跑完所有 case。
// 失败不中断（继续跑后面的 case），最后按失败数决定退出码——CI 里看退出码就够。
namespace tf {

inline int& checks() {
    static int n = 0;
    return n;
}

inline int& failures() {
    static int n = 0;
    return n;
}

inline void record(bool ok, const char* expr, const char* file, int line) {
    ++checks();
    if (!ok) {
        ++failures();
        std::printf("    FAIL %s:%d  %s\n", file, line, expr);
    }
}

inline void caseName(const char* name) { std::printf("  · %s\n", name); }

inline int summary(const char* suite) {
    const int f = failures();
    std::printf("\n%s: %d checks, %d failed\n", suite, checks(), f);
    return f == 0 ? 0 : 1;
}

}  // namespace tf

#define CHECK(cond) ::tf::record((cond), #cond, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, eps) \
    ::tf::record(std::fabs(double(a) - double(b)) <= (eps), #a " ≈ " #b, __FILE__, __LINE__)
#define TF_CASE(name) ::tf::caseName(name)

// 各测试文件对外只暴露一个入口
void testMath();
void testRaster();
void testClip();
void testTexture();
