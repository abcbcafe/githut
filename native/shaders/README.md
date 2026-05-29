# Shaders

Ray-tracing and compute shaders for the path tracer, compiled to SPIR-V.

Planned stages (added across M1+, see `docs/ROADMAP.md`):

| File | Stage | Purpose |
|---|---|---|
| `raygen.rgen` | ray generation | primary rays, camera, writes the G-buffer |
| `closest_hit.rchit` | closest hit | surface attributes, PBR BSDF evaluation |
| `miss.rmiss` | miss | sky / environment lookup |
| `shadow.rmiss` | miss | shadow-ray visibility |
| `skin.comp` | compute | GPU skinning → deformed vertex/normal buffer (M3) |
| `restir_di.comp` | compute | ReSTIR DI temporal + spatial reuse (M4) |
| `accumulate.comp` | compute | temporal accumulation |
| `acoustic.rgen` | ray generation | geometric acoustic ray tracing (M5) |

Authored in **Slang** or **GLSL**, targeting `vulkan1.3`. Until the CMake shader
step is wired up, compile manually, e.g.:

```bash
glslangValidator --target-env vulkan1.3 -V raygen.rgen -o raygen.rgen.spv
```

Compiled `*.spv` files are git-ignored.
