#pragma once

// A small unidirectional Monte Carlo path tracer over a TriangleScene. Diffuse
// (Lambertian) bounces with cosine-importance sampling, emissive surfaces, a
// constant environment radiance, and Russian-roulette termination. This validates
// the integration/sampling math the GPU path tracer will reuse; correctness is
// checked with white-furnace energy-conservation tests.

#include <cstdint>
#include <vector>

#include "core/math.h"
#include "core/sampling.h"
#include "render/camera.h"
#include "render/cpu_reference.h"
#include "render/medium.h"
#include "voxel/material_palette.h"

namespace pathtracer::render {

struct PathSettings {
    int width = 160;
    int height = 90;
    int spp = 16;          // samples per pixel
    int max_depth = 16;    // max bounces
    core::Vec3 env_radiance{0.0f, 0.0f, 0.0f}; // constant background/illumination

    // Homogeneous "air" volumetrics applied as distance fog / aerial perspective on
    // the primary segment: L = T(d)*L_surface + (1 - T(d))*fog_inscatter, where d is
    // the camera-to-surface distance and T is Beer-Lambert transmittance. The medium
    // primitives (medium.h) also back the future GPU volumetric scattering path.
    bool medium_enabled = false;
    HomogeneousMedium medium;
    core::Vec3 fog_inscatter{0.0f, 0.0f, 0.0f};

    // Next-event estimation: directly sample emissive voxels each bounce and combine
    // with BSDF sampling via multiple importance sampling (balance heuristic). Same
    // result as pure BSDF sampling in expectation, with much lower variance. Requires
    // scene.build_lights() to have been called.
    bool next_event_estimation = false;
};

// Estimate incoming radiance along a single primary ray. When out_primary_t is
// provided it receives the camera-to-first-surface distance (-1 if the primary ray
// misses), used by the caller to apply distance fog.
core::Vec3 trace_path(const TriangleScene &scene, const voxel::MaterialPalette &palette,
                      core::Ray ray, core::Pcg32 &rng, int max_depth,
                      const core::Vec3 &env_radiance, float *out_primary_t = nullptr);

// As trace_path, but with next-event estimation + MIS (balance heuristic) for the
// scene's emissive area lights. Unbiased; lower variance than trace_path.
core::Vec3 trace_path_nee(const TriangleScene &scene, const voxel::MaterialPalette &palette,
                          core::Ray ray, core::Pcg32 &rng, int max_depth,
                          const core::Vec3 &env_radiance, float *out_primary_t = nullptr);

// Render a full framebuffer (row-major linear RGB). Deterministic for a given seed.
std::vector<core::Vec3> path_render(const TriangleScene &scene, const PinholeCamera &camera,
                                    const voxel::MaterialPalette &palette,
                                    const PathSettings &settings, uint64_t seed);

} // namespace pathtracer::render
