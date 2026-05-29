#include "doctest.h"

#include <cmath>

#include "render/camera.h"
#include "render/cpu_reference.h"
#include "render/path_integrator.h"
#include "voxel/greedy_mesher.h"
#include "voxel/material_palette.h"
#include "voxel/voxel_chunk.h"

using namespace pathtracer;
using pathtracer::render::PathSettings;
using pathtracer::render::PinholeCamera;
using pathtracer::render::TriangleScene;

namespace {

void box(voxel::VoxelChunk &c, int x0, int y0, int z0, int x1, int y1, int z1,
         voxel::MaterialId m) {
    for (int z = z0; z <= z1; ++z)
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) c.set(x, y, z, m);
}

// An emissive wall at z=0, a 1-voxel-thick glass slab at z=5, a gap between, viewed
// head-on from +z. The center rays pass straight through the slab (normal incidence)
// to the wall. `attenuation` tints the glass.
TriangleScene glass_in_front_of_wall(voxel::MaterialPalette &palette, float emission,
                                     std::array<float, 3> attenuation) {
    voxel::PbrMaterial wall;
    wall.albedo = {0.0f, 0.0f, 0.0f};
    wall.emission = {emission, emission, emission};
    const auto wall_id = palette.add(wall);

    voxel::PbrMaterial glass;
    glass.transmission = 1.0f;
    glass.ior = 1.5f;
    glass.attenuation = attenuation;
    const auto glass_id = palette.add(glass);

    voxel::VoxelChunk chunk;
    box(chunk, 0, 0, 0, 11, 11, 0, wall_id);  // emissive wall, +z face at z=1
    box(chunk, 3, 3, 5, 8, 8, 5, glass_id);   // glass slab, z in [5,6]

    TriangleScene scene;
    scene.add_mesh(voxel::greedy_mesh(chunk));
    scene.build_bvh();
    return scene;
}

PinholeCamera head_on() {
    return PinholeCamera({5.5f, 5.5f, 20.0f}, {0, 0, -1}, {0, 1, 0}, 0.25f, 1.0f);
}

// Mean of a centered square region of the framebuffer.
double center_mean_x(const std::vector<core::Vec3> &fb, int w, int h, int half) {
    double sum = 0.0;
    int n = 0;
    for (int y = h / 2 - half; y <= h / 2 + half; ++y)
        for (int x = w / 2 - half; x <= w / 2 + half; ++x) {
            sum += fb[y * w + x].x;
            ++n;
        }
    return sum / n;
}

} // namespace

TEST_CASE("clear glass transmits an emitter minus Fresnel reflections") {
    voxel::MaterialPalette palette;
    const float E = 1.0f;
    const TriangleScene scene = glass_in_front_of_wall(palette, E, {0.0f, 0.0f, 0.0f});

    PathSettings s;
    s.width = 41;
    s.height = 41;
    s.spp = 160;
    s.max_depth = 8;
    s.env_radiance = {0.0f, 0.0f, 0.0f}; // reflected rays escape to black

    const auto fb = path_render(scene, head_on(), palette, s, 3);
    // Normal incidence: R = 0.04 at each of the two interfaces; the (eta_i/eta_t)^2
    // factors cancel over the round trip, so transmitted radiance ~ (1-R)^2 * E.
    const double expected = 0.96 * 0.96 * E;
    CHECK(center_mean_x(fb, s.width, s.height, 4) == doctest::Approx(expected).epsilon(0.04));
}

TEST_CASE("tinted glass attenuates by Beer-Lambert through its thickness") {
    voxel::MaterialPalette palette;
    const float E = 1.0f;
    const float sigma = 0.6f; // per unit length; slab is 1 voxel thick
    const TriangleScene scene = glass_in_front_of_wall(palette, E, {sigma, sigma, sigma});

    PathSettings s;
    s.width = 41;
    s.height = 41;
    s.spp = 200;
    s.max_depth = 8;
    s.env_radiance = {0.0f, 0.0f, 0.0f};

    const auto fb = path_render(scene, head_on(), palette, s, 9);
    const double expected = 0.96 * 0.96 * std::exp(-sigma * 1.0f) * E;
    CHECK(center_mean_x(fb, s.width, s.height, 4) == doctest::Approx(expected).epsilon(0.05));
}

TEST_CASE("opaque-vs-glass: glass lets the wall show through") {
    voxel::MaterialPalette palette;
    const TriangleScene scene = glass_in_front_of_wall(palette, 1.0f, {0.0f, 0.0f, 0.0f});
    PathSettings s;
    s.width = 31;
    s.height = 31;
    s.spp = 64;
    s.env_radiance = {0.0f, 0.0f, 0.0f};
    const auto fb = path_render(scene, head_on(), palette, s, 1);
    // The wall behind the glass is clearly visible (most light transmits).
    CHECK(center_mean_x(fb, s.width, s.height, 3) > 0.8);
}
