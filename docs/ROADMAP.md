# Roadmap

Each milestone has a single demonstrable deliverable. Risk is front-loaded:
**M0 (bridge) > M4 (ReSTIR) > M3 (skinned refit) > M2 > M5 > M1.**

## M0 — De-risk the bridge  *(in progress)*
Highest uncertainty; throwaway spikes allowed.
- [x] Repo restructured; legacy site moved to `legacy-githut/`.
- [x] Project scaffold: Godot 4.6 project, C++ GDExtension (CMake + godot-cpp), `.gdextension`.
- [ ] GDExtension loads in Godot 4.6 and registers the `PathTracedViewport` node.
- [ ] Extension creates its own RT-enabled `VkDevice`; confirm RT extensions present.
- [ ] External-memory `VkImage` + timeline-semaphore handoff; Godot draws it as `Texture2DRD`.
- **Deliverable:** an animated RT-rendered triangle visible inside Godot, composited under UI.

## M1 — Minimal path tracer, static triangle scene
BLAS/TLAS, pinhole raygen, primary-hit albedo, hardcoded sun + NEE shadows, temporal
accumulation, GGX PBR; load a glTF test mesh + materials.
- [x] CPU math core: `Vec3`, `Ray`, `Aabb`, Möller–Trumbore ray/triangle, ray/AABB slab test
  (`native/src/core/math.h`) — reused by the renderer and the acoustic tracer.
- [x] `PinholeCamera` primary-ray generation (matches the planned raygen math), unit-tested.
- [x] CPU BVH (analogue of the GPU BLAS), verified against brute force; PCG RNG + cosine
  hemisphere sampling; Monte Carlo diffuse **path integrator** with Russian roulette, verified by
  white-furnace energy-conservation tests (`native/src/render/{bvh,path_integrator}.cpp`).
- [x] GGX microfacet **BSDF** module (NDF + Smith + Fresnel-Schlick + importance sampling),
  verified by NDF-normalization, Smith-bounds, Fresnel-endpoint, and single-scatter energy tests
  (`native/src/render/bsdf.h`). Ready to wire into the integrator/GPU.
- [x] **Next-event estimation + MIS** (balance heuristic) sampling emissive voxels as area lights,
  verified by equality-in-expectation against the BSDF-only path tracer and a variance-reduction
  test (`trace_path_nee`, `TriangleScene::{build,sample}_light*`). This is the precursor to the
  GPU ReSTIR DI work.
- [x] Smooth **dielectric glass**: Snell refraction, unpolarized Fresnel, total internal
  reflection, radiance scaling across the boundary, and Beer–Lambert tint inside the glass
  (`bsdf::{fresnel_dielectric,refract}`, `dielectric_bounce`). Verified by Snell/Fresnel/TIR unit
  tests and integrator transmittance tests (`--glass`).
- [ ] GPU: BLAS/TLAS + RT pipeline; primary-hit albedo; NEE shadows; temporal accumulation; GGX BSDF.

### Volumetrics ("air") — CPU groundwork
- [x] Homogeneous-medium primitives: Beer–Lambert transmittance, free-flight distance sampling,
  Henyey–Greenstein phase (eval + sampling), verified by transmittance/mean-free-path and phase
  normalization/mean-cosine tests (`native/src/render/medium.h`).
- [x] Distance fog / aerial perspective in the path integrator, verified by a Beer–Lambert
  attenuation test (`--pt --fog`).
- [ ] GPU: full volumetric scattering (delta tracking + HG multiple scattering) using these
  primitives; participating media tied to acoustic absorption later.
- **Deliverable:** a static PBR scene converging cleanly via accumulation, camera fly-through.

## M2 — Voxel world
Chunk data, greedy mesh → BLAS, `.vox` import, material palette, Jolt collider per chunk, many
TLAS chunk instances.
- [x] CPU core: `VoxelChunk` (dense 32³), `MaterialPalette`, greedy mesher → `MeshData`.
- [x] Unit tests (doctest) covering meshing invariants, face merging, material boundaries.
- [ ] Build BLAS per chunk from `MeshData`; chunk → TLAS instances.
- [x] MagicaVoxel `.vox` import → palette + chunks (`vox_loader`), unit-tested end-to-end.
- [ ] Jolt static collider per chunk.
- **Deliverable:** walk a coarse-voxel level with per-material PBR.

## M3 — Skinned Mixamo NPCs
FBX import → Skeleton3D/AnimationPlayer, compute skinning → deformed buffer, BLAS refit, NPC
instances in TLAS.
- **Deliverable:** an animated Mixamo character walking in the voxel level, correctly lit/shadowed.

## M4 — Lighting quality
Emissive voxels as area lights, ReSTIR DI (temporal + spatial reuse), OIDN with albedo/normal
aux, then ReSTIR GI.
- **Deliverable:** stable 1 spp + denoise interior with many emissive sources, interactive rates.

## M5 — Custom spatial audio
Acoustic ray tracer on the shared AS, IR + convolution reverb, UTD diffraction, HRTF binaural;
acoustic material coefficients on the voxel palette.
- **Deliverable:** gunshots/footsteps occlude through walls and reverberate per-room, binaural.

## M6 — FPS gameplay vertical slice (GoldenEye feel)
Weapons (hitscan + projectile), enemy AI (NavigationServer3D patrol/alert/attack), objectives,
damage, stealth (sight + sound detection reusing audio occlusion), HUD.
- **Deliverable:** one playable objective-based level (infiltrate → objective → fight/evade → exit).

## M7 — Polish / stretch
ReSTIR PT/RTXPT, OptiX denoiser, glass/transparency, in-editor voxel editor, CLAS for animated
geometry, live path-traced editor viewport.

## Verification
- **M0:** run project; RT texture animates with no validation errors (`VK_LAYER_KHRONOS_validation`
  on in debug) and no tearing under the UI.
- **Per-milestone smoke scene** under `game/scenes/` exercising that milestone's feature.
- **Renderer correctness:** the CPU reference path tracer (`native/tools/cpu_reference`, already
  available) produces golden images from the same voxel/material data; compare against the GPU
  path tracer's 1 spp + denoise per smoke scene.
- **Skinning:** toggle animation; confirm shadows/GI track the mesh; no topology-change asserts.
- **Audio:** A/B room with vs. without walls; occlusion + reverb-tail length scale with room size;
  binaural localization check with a moving source.
- **Gameplay:** play the M6 level end-to-end.
- **CI:** CMake build of the extension on Linux x86_64; `godot --headless` smoke load to confirm
  the extension registers.

## Open items (defaults chosen)
- **Editor-live viewport:** runtime-first; live-in-editor deferred to M7.
- **Multiplayer/split-screen:** single-player for the slice.
- **Single-GPU:** external-memory path assumes Godot + RT renderer share one RTX 4090+.
- **Denoiser:** OIDN first; OptiX is an M7 upgrade.
