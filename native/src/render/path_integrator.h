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
#include "voxel/material_palette.h"

namespace pathtracer::render {

struct PathSettings {
    int width = 160;
    int height = 90;
    int spp = 16;          // samples per pixel
    int max_depth = 16;    // max bounces
    core::Vec3 env_radiance{0.0f, 0.0f, 0.0f}; // constant background/illumination
};

// Estimate incoming radiance along a single primary ray.
core::Vec3 trace_path(const TriangleScene &scene, const voxel::MaterialPalette &palette,
                      core::Ray ray, core::Pcg32 &rng, int max_depth,
                      const core::Vec3 &env_radiance);

// Render a full framebuffer (row-major linear RGB). Deterministic for a given seed.
std::vector<core::Vec3> path_render(const TriangleScene &scene, const PinholeCamera &camera,
                                    const voxel::MaterialPalette &palette,
                                    const PathSettings &settings, uint64_t seed);

} // namespace pathtracer::render
