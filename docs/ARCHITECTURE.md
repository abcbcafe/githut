# Architecture

This document describes how a custom Vulkan hardware ray-traced renderer and a custom acoustic
ray tracer coexist with Godot 4.6.

## Decisions

| Area | Decision | Rationale |
|---|---|---|
| Native language | **C++** with `godot-cpp` | Every reference (RTXDI, RTXPT, nvpro-samples, OIDN, OptiX) is C++ |
| Integration | In-process **GDExtension owning its own RT-enabled `VkDevice`** | Godot can't enable RT extensions on its device post-creation; keep all Godot tooling |
| Audio | **Custom geometric acoustic ray tracer** reusing the renderer's acceleration structures | Matches the "write our own" spirit; unifies geometry |
| Voxels | **Greedy-mesh chunks into triangle BLAS** | Voxels + skinned NPCs share one hardware-RT triangle pipeline |
| Shaders | Slang or GLSL → SPIR-V | RT stages; Slang increasingly the SOTA choice |
| Build | CMake | godot-cpp + Vulkan-Hpp + VMA + OIDN |

## Why a separate Vulkan device

Godot's `RenderingDevice` exposes compute and raster but **not** the Vulkan ray-tracing
extensions, and a device's enabled extensions are fixed at `vkCreateDevice` time. Therefore we
cannot reuse Godot's `VkDevice` for RT. Instead our extension creates its own RT-enabled device on
the *same physical GPU* and shares only the final rendered image back to Godot.

Godot 4.3+ exposes `RenderingDevice.get_driver_resource()` (instance / physical device / device /
queue) and `texture_create_from_extension` / `Texture2DRD`, which let Godot import an external
`VkImage`. This is the basis of the zero-copy presentation bridge.

## The presentation bridge (the highest-risk piece — M0)

1. Our RT device allocates the render target `VkImage` backed by `VK_KHR_external_memory`
   (opaque-FD on Linux).
2. The opaque FD is handed to Godot, which imports it via `texture_create_from_extension` and wraps
   it as a `Texture2DRD` drawn fullscreen under the `Control`-node UI.
3. Cross-device synchronization uses a shared **timeline semaphore**
   (`VK_KHR_external_semaphore`, opaque FD): the RT device signals when a frame is ready; Godot
   waits before sampling.
4. Godot's own 3D draw renders an empty world; visual nodes exist only as data carriers for the
   scene extractor.

**Fallback** if interop proves troublesome: CPU readback from the RT device + upload into a Godot
texture each frame (correct but slow — acceptable since performance is not an early priority).

## Per-frame scene extraction (CPU → our device buffers)

- **Camera:** transform + fov from the active `Camera3D`.
- **TLAS instances:** transforms for voxel chunks + static/skinned meshes (small, every frame).
- **Skinned NPCs:** bone matrices from `Skeleton3D.get_bone_global_pose()` feed GPU skinning.
- **Lights:** Godot light nodes + emissive materials populate the light buffer.
- **Static bulk data** (voxel meshes, textures, base skinned meshes) uploaded once at load and
  converted from `BaseMaterial3D` / `ArrayMesh` into a single canonical material struct + a
  bindless texture array at import time.

## Keep vs. replace

| Subsystem | Decision |
|---|---|
| Scene tree, editor + import (FBX/glTF/textures), Jolt physics, NavigationServer3D, Skeleton3D/AnimationPlayer, input, UI | **Keep** |
| 3D rendering (RenderingServer) | **Replace** — our path tracer; Godot only blits our texture + UI |
| Audio spatialization | **Replace** — custom acoustic tracer; Godot used only for asset load + event triggers |

## Subsystem designs

### Voxel world
- 32³ voxel chunks; dense `u8`/`u16` material-id array per chunk (palette compression later).
- Greedy meshing keyed on (material id, face normal) → triangle buffer → one BLAS per chunk; the
  world is a set of TLAS instances. Editing a chunk re-meshes it and rebuilds that BLAS.
- A global voxel material palette maps id → PBR material; per-face emissive ids let voxels act as
  area lights. The same greedy mesh feeds Jolt as a static `ConcavePolygonShape3D`.
- Authoring starts with MagicaVoxel `.vox` import; an in-editor voxel editor comes later.

### Path tracer
- BLAS per chunk/mesh; per-frame BLAS refit for skinned NPCs; TLAS rebuilt per frame.
- Ray pipeline: raygen → primary-hit G-buffer (position/normal/material id/motion vector) →
  ReSTIR DI direct lighting → ReSTIR GI indirect; `ray_query` in compute for visibility rays.
- Ordering: temporal accumulation → ReSTIR DI → OIDN denoise → ReSTIR GI.
- BSDF: metallic-roughness PBR (GGX + multiscatter, Lambertian/Oren-Nayar, Fresnel-Schlick),
  1:1 with Godot `BaseMaterial3D`. Glass/transmission deferred.
- Lights: analytic sun + procedural sky; emissive triangles as area lights; explicit point/spot.
- **Volumetrics ("air"):** a homogeneous participating medium (per-channel absorption/scattering +
  Henyey–Greenstein phase). The CPU reference starts with exact Beer–Lambert distance fog / aerial
  perspective; the GPU path will do full volumetric scattering (delta tracking + HG multiple
  scattering) using the same primitives (`native/src/render/medium.h`). The medium's absorption
  later ties into the acoustic model.

### Skinned Mixamo pipeline
- Mixamo FBX → Godot importer → `Skeleton3D` + `AnimationPlayer` (preset: scale 0.01, root motion).
- Base mesh stored once; a compute shader applies bone matrices → deformed vertex/normal buffer.
- BLAS built with `ALLOW_UPDATE_BIT_KHR` + `PREFER_FAST_BUILD_BIT_KHR`; refit each frame (mode
  `UPDATE`). Topology is fixed per BLAS — LOD switches require a rebuild.

### Audio (custom geometric acoustic tracer)
- Interface: `propagate(listener, sources, scene) -> per-source IR + params`.
- v0 stub: Godot `AudioStreamPlayer3D` so gameplay isn't blocked.
- v1: acoustic rays against the *same* TLAS/BLAS → per-band energy paths → impulse response;
  UTD edge diffraction; partitioned-convolution reverb; HRTF binaural (SOFA dataset).
- Voxel material palette gains per-band absorption/scattering coefficients. Stealth AI reuses the
  occlusion result for "can the guard hear this?".
