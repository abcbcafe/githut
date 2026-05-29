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
- **Deliverable:** a static PBR scene converging cleanly via accumulation, camera fly-through.

## M2 — Voxel world
Chunk data, greedy mesh → BLAS, `.vox` import, material palette, Jolt collider per chunk, many
TLAS chunk instances.
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
- **Renderer correctness:** offline high-spp golden image vs. 1 spp + denoise per smoke scene.
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
