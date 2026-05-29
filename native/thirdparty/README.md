# Third-party dependencies

These are git submodules; their sources are not vendored into this repo. Run:

```bash
git submodule update --init --recursive
```

| Submodule | Branch/Pin | Role |
|---|---|---|
| `godot-cpp` | `4.5` (forward-compatible with Godot 4.6) | C++ GDExtension bindings |
| `VulkanMemoryAllocator` | latest | GPU allocations + external-memory helpers |
| `oidn` *(added at M4)* | release | Intel Open Image Denoise |

The Vulkan SDK itself is **not** a submodule — install the LunarG SDK on the build machine and let
CMake's `find_package(Vulkan)` locate it (see `docs/BUILD.md`).
