#include "doctest.h"

#include <cmath>

#include "render/caustics.h"
#include "render/cpu_reference.h"
#include "voxel/greedy_mesher.h"
#include "voxel/material_palette.h"
#include "voxel/voxel_chunk.h"

using namespace pathtracer;
using pathtracer::core::Vec3;
using pathtracer::render::CausticMap;
using pathtracer::render::TriangleScene;

TEST_CASE("CausticMap accumulates flux and reports irradiance = flux / cell area") {
    // 4x4 grid over a 4x4 area => each cell is 1x1 (area 1).
    CausticMap map(0.0f, 0.0f, 4.0f, 4.0f, 4, 0.0f);
    map.splat({0.5f, 0.0f, 0.5f}, {2.0f, 0.0f, 0.0f});
    map.splat({0.5f, 0.0f, 0.5f}, {1.0f, 0.0f, 0.0f}); // same cell
    CHECK(map.irradiance({0.5f, 0.0f, 0.5f}).x == doctest::Approx(3.0f)); // flux 3 / area 1
    CHECK(map.irradiance({3.5f, 0.0f, 3.5f}).x == doctest::Approx(0.0f)); // empty cell
    CHECK(map.irradiance({-1.0f, 0.0f, 0.0f}).x == doctest::Approx(0.0f)); // out of bounds
    CHECK(map.total_flux().x == doctest::Approx(3.0f));
}

TEST_CASE("CausticMap merge sums two maps of identical layout") {
    CausticMap a(0, 0, 2, 2, 2, 0), b(0, 0, 2, 2, 2, 0);
    a.splat({0.5f, 0, 0.5f}, {1, 0, 0});
    b.splat({0.5f, 0, 0.5f}, {2, 0, 0});
    a.merge(b);
    CHECK(a.total_flux().x == doctest::Approx(3.0f));
}

namespace {
// An emissive ceiling panel, a clear glass sphere below it, and a diffuse floor.
TriangleScene caustic_scene(voxel::MaterialPalette &palette) {
    voxel::PbrMaterial floor;
    floor.albedo = {0.8f, 0.8f, 0.8f};
    const auto floor_id = palette.add(floor);
    voxel::PbrMaterial light;
    light.emission = {40.0f, 40.0f, 40.0f};
    const auto light_id = palette.add(light);
    voxel::PbrMaterial glass;
    glass.transmission = 1.0f;
    glass.ior = 1.5f;
    glass.roughness = 0.0f;
    const auto glass_id = palette.add(glass);

    voxel::VoxelChunk chunk;
    for (int z = 0; z < 20; ++z)
        for (int x = 0; x < 20; ++x) chunk.set(x, 0, z, floor_id); // floor at y in [0,1]
    for (int z = 8; z < 12; ++z)
        for (int x = 8; x < 12; ++x) chunk.set(x, 18, z, light_id); // ceiling panel

    TriangleScene scene;
    scene.add_mesh(voxel::greedy_mesh(chunk));
    scene.build_bvh();
    scene.build_lights(palette);
    scene.add_sphere({10.0f, 8.0f, 10.0f}, 3.0f, glass_id); // glass sphere above the floor
    return scene;
}
} // namespace

TEST_CASE("a glass sphere focuses light into a caustic (peak >> mean) and conserves energy") {
    voxel::MaterialPalette palette;
    const TriangleScene scene = caustic_scene(palette);

    CausticMap map(0.0f, 0.0f, 20.0f, 20.0f, 200, 1.0f);
    render::trace_caustics(scene, palette, map, 600000, 1);

    const Vec3 total = map.total_flux();
    CHECK(total.x > 0.0f); // some caustic flux was deposited

    // Energy bound: deposited flux cannot exceed the emitted power pi*Le*A.
    const float emitted = 3.14159265f * 40.0f * scene.total_emissive_area();
    CHECK(total.x <= emitted * 1.01f);

    // Focusing: the brightest cell is far above the average lit cell (a caustic).
    CHECK(map.peak_irradiance() > 4.0 * map.mean_nonzero_irradiance());
}
