# Standalone unit-test project for the GPU-free CPU core (voxel engine, greedy
# mesher, material palette). Deliberately independent of godot-cpp and Vulkan so
# it builds and runs anywhere — including CPU-only CI — without a GPU or the
# Vulkan SDK.
#
#   cmake -S native/tests -B native/build-tests
#   cmake --build native/build-tests -j
#   ctest --test-dir native/build-tests --output-on-failure

cmake_minimum_required(VERSION 3.24)
project(pathtracer_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

find_package(Threads REQUIRED)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

add_executable(pathtracer_tests
    test_main.cpp
    test_voxel_chunk.cpp
    test_greedy_mesher.cpp
    test_math.cpp
    test_vox_loader.cpp
    test_cpu_reference.cpp
    test_sampling.cpp
    test_bvh.cpp
    test_path_integrator.cpp
    test_medium.cpp
    test_bsdf.cpp
    test_nee.cpp
    test_dielectric.cpp
    test_glass.cpp
    # CPU core under test (must stay free of Godot/Vulkan includes):
    "${SRC_DIR}/voxel/greedy_mesher.cpp"
    "${SRC_DIR}/voxel/vox_loader.cpp"
    "${SRC_DIR}/render/cpu_reference.cpp"
    "${SRC_DIR}/render/bvh.cpp"
    "${SRC_DIR}/render/path_integrator.cpp")

target_include_directories(pathtracer_tests PRIVATE
    "${SRC_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")

target_link_libraries(pathtracer_tests PRIVATE Threads::Threads)

if(NOT MSVC)
    target_compile_options(pathtracer_tests PRIVATE -Wall -Wextra)
endif()

add_test(NAME pathtracer_unit_tests COMMAND pathtracer_tests)
