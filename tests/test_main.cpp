#include "test_framework.h"

// 每个测试文件暴露一个入口，这里按顺序跑。
// 退出码 = 是否有失败，方便 `ctest` 或 CI 直接判定。
int main() {
    testMath();
    testRaster();
    testClip();
    testTexture();
    return tf::summary("soft_raster tests");
}
