cmake_minimum_required(VERSION 3.16)
project(soft_raster CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_executable(soft_raster src/main.cpp)
target_link_libraries(soft_raster PRIVATE SDL2::SDL2main SDL2::SDL2)