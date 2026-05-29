#!/usr/bin/env bash
# Build and run the GPU-free CPU-core unit tests (voxel engine, greedy mesher,
# material palette). Requires only CMake + a C++20 compiler — no GPU or Vulkan SDK.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/native/build-tests"

cmake -S "${ROOT}/native/tests" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug
cmake --build "${BUILD_DIR}" -j

ctest --test-dir "${BUILD_DIR}" --output-on-failure
