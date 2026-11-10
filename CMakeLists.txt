cmake_minimum_required(VERSION 3.16)
project(soft_raster CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 必须！Debug 下的性能数字毫无意义（指导 §3.2）
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)

# ---------------------------------------------------------------- 核心库
# 不依赖 SDL：渲染管线本身是纯 C++，这样 bench/test 都能在没有窗口的环境里跑。
# 每完成一天，把新文件加进这个列表（只加不改）。
add_library(sr_core
  src/core/framebuffer.cpp
  src/pipeline/rasterizer.cpp 
)

target_include_directories(sr_core PUBLIC src third_party)
target_compile_options(sr_core PRIVATE -O3 -Wall -Wextra -Wshadow)

# ---------------------------------------------------------------- 可执行文件
# SDL 只出现在 platform/ 这一层：开窗、收输入、把 framebuffer 贴成纹理。
add_executable(soft_raster
  src/main.cpp
  src/platform/window_sdl.cpp
)
target_link_libraries(soft_raster PRIVATE sr_core SDL2::SDL2)
target_compile_options(soft_raster PRIVATE -Wall -Wextra -Wshadow)

# ---------------------------------------------------------------- 后续几天启用
# D5：单元测试骨架
# enable_testing()
# add_executable(test_all tests/test_math.cpp tests/test_raster.cpp tests/test_clip.cpp)
# target_link_libraries(test_all PRIVATE sr_core)
# add_test(NAME unit COMMAND test_all)

# D6：benchmark
# add_executable(bench bench/bench.cpp)
# target_link_libraries(bench PRIVATE sr_core)
