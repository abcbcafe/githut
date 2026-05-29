#!/bin/bash
# SessionStart hook for Claude Code on the web.
#
# The CPU test harness (native/tests, run via scripts/run_tests.sh) needs only a
# C++20 toolchain: CMake + g++ + make. This installs them if missing and warms the
# test build configuration so `ctest` / scripts/run_tests.sh work immediately.
#
# The full native GDExtension (godot-cpp + Vulkan + RTX) is NOT built here — it
# requires a GPU and the Vulkan SDK and is built on a workstation (see docs/BUILD.md).
set -euo pipefail

# Only needed in the remote (web) environment; local machines have their own setup.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"

# 1. Ensure the C++ build toolchain is present.
need_install=0
for tool in cmake g++ make; do
  command -v "$tool" >/dev/null 2>&1 || need_install=1
done

if [ "$need_install" -eq 1 ]; then
  SUDO=""
  [ "$(id -u)" -ne 0 ] && SUDO="sudo"
  $SUDO apt-get update -y
  $SUDO apt-get install -y --no-install-recommends cmake g++ make
fi

# 2. Warm the CPU test build configuration (idempotent).
cmake -S "$PROJECT_DIR/native/tests" -B "$PROJECT_DIR/native/build-tests" >/dev/null

echo "session-start: C++ toolchain ready; CPU test build configured."
