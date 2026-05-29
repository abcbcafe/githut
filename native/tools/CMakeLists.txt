# Standalone CPU tools (no GPU / Vulkan / godot-cpp). Builds the offline reference
# renderer used to generate golden images.
#
#   cmake -S native/tools -B native/build-tools
#   cmake --build native/build-tools -j
#   ./native/build-tools/cpu_reference out.ppm [scene.vox]

cmake_minimum_required(VERSION 3.24)
project(pathtracer_tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

add_executable(cpu_reference
    cpu_reference_main.cpp
    "${SRC_DIR}/voxel/greedy_mesher.cpp"
    "${SRC_DIR}/voxel/vox_loader.cpp"
    "${SRC_DIR}/render/cpu_reference.cpp")

target_include_directories(cpu_reference PRIVATE "${SRC_DIR}")

if(NOT MSVC)
    target_compile_options(cpu_reference PRIVATE -Wall -Wextra)
endif()
