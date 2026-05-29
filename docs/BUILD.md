# Build guide

> **Note:** the native extension links against the Vulkan SDK and a GPU toolchain and is intended
> to be built on a developer workstation (or a GPU-equipped CI runner), **not** in a CPU-only
> container. Source scaffolding can be edited anywhere, but compilation and running require the
> prerequisites below.

## Prerequisites (Linux x86_64)

- **Godot 4.6.x** (editor + headless binary).
- **CMake ≥ 3.24** and a C++20 compiler (GCC 13+ or Clang 16+).
- **Vulkan SDK** with ray-tracing support (LunarG SDK) providing Vulkan-Hpp and validation layers.
  Verify `VK_KHR_ray_tracing_pipeline`, `VK_KHR_acceleration_structure`, `VK_KHR_ray_query` are
  present on your RTX 4090+ (`vulkaninfo | grep -i ray_tracing`).
- **NVIDIA driver** new enough for the above (RTX 40-series or newer).
- A SPIR-V shader compiler: **glslang** (`glslangValidator`) and/or the **Slang** compiler.
- (Later milestones) **Intel OIDN** and optionally **NVIDIA OptiX / CUDA** for denoising.

## Submodules

Third-party C++ dependencies live under `native/thirdparty/` as git submodules:

- `godot-cpp` — the official C++ bindings. Pinned to the **`4.5` branch** because godot-cpp has
  no `4.6` branch yet; GDExtensions are forward-compatible (an extension built against an older API
  loads on a newer engine, as long as runtime version ≥ API version), so a 4.5-API extension runs
  fine on Godot 4.6. Switch the submodule to `4.6` once that branch is published.
- `VulkanMemoryAllocator` (VMA) — GPU allocations + external-memory helpers.
- `oidn` — Intel Open Image Denoise (added at M4).

```bash
git submodule update --init --recursive
```

## Configure & build

```bash
cmake -S native -B native/build -DCMAKE_BUILD_TYPE=Debug
cmake --build native/build -j
```

The build produces a shared library under `native/build/` and (via a post-build copy) places it in
`game/addons/pathtracer/bin/` where `pathtracer.gdextension` expects it.

## Run

```bash
# Editor
godot --path game

# Headless smoke test (confirms the extension registers without error)
godot --headless --path game --quit
```

## Compiling shaders

RT and compute shaders in `native/shaders/` are compiled to SPIR-V at build time. Until the CMake
shader step is wired up you can compile manually, e.g.:

```bash
glslangValidator --target-env vulkan1.3 -V native/shaders/raygen.rgen -o native/shaders/raygen.rgen.spv
```

## Troubleshooting

- **Extension fails to load:** confirm the `compatibility_minimum` in `pathtracer.gdextension`
  matches your Godot version and that the `.so` path is correct.
- **No RT extensions:** make sure you are on the discrete RTX GPU (not an iGPU) and the LunarG SDK
  is on `VULKAN_SDK` / `LD_LIBRARY_PATH`.
- **Validation errors:** debug builds enable `VK_LAYER_KHRONOS_validation`; fix all reported issues
  before moving on (especially around external-memory import and semaphore signaling).
