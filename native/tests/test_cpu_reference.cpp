#include "doctest.h"

#include "render/cpu_reference.h"
#include "voxel/greedy_mesher.h"
#include "voxel/material_palette.h"
#include "voxel/voxel_chunk.h"

using namespace pathtracer;
using pathtracer::core::Ray;
using pathtracer::render::PinholeCamera;
using pathtracer::render::RenderSettings;
using pathtracer::render::TriangleScene;

namespace {

// A single solid voxel occupying [4,5]^3 with material id 1.
TriangleScene single_voxel_scene(voxel::MaterialPalette &palette) {
    palette.add(voxel::PbrMaterial{}); // id 1, default gray albedo
    voxel::VoxelChunk chunk;
    chunk.set(4, 4, 4, 1);
    TriangleScene scene;
    scene.add_mesh(voxel::greedy_mesh(chunk));
    return scene;
}

} // namespace

TEST_CASE("scene is built from greedy mesh triangles") {
    voxel::MaterialPalette palette;
    const TriangleScene scene = single_voxel_scene(palette);
    CHECK(scene.triangle_count() == 12); // 6 faces * 2 triangles
}

TEST_CASE("closest_hit reports the front face, material, and distance") {
    voxel::MaterialPalette palette;
    const TriangleScene scene = single_voxel_scene(palette);

    // Look at the +z face (voxel spans z in [4,5]) from z = 10 along -z.
    const Ray ray{{4.5f, 4.5f, 10.0f}, {0, 0, -1}};
    const auto hit = scene.closest_hit(ray);
    REQUIRE(hit.hit);
    CHECK(hit.material == 1);
    CHECK(hit.t == doctest::Approx(5.0f)); // 10 - 5
    CHECK(hit.normal.z == doctest::Approx(1.0f));
}

TEST_CASE("any_hit honors the max distance (shadow rays)") {
    voxel::MaterialPalette palette;
    const TriangleScene scene = single_voxel_scene(palette);
    const Ray ray{{4.5f, 4.5f, 10.0f}, {0, 0, -1}};
    CHECK(scene.any_hit(ray, 1e30f));   // voxel is in range
    CHECK_FALSE(scene.any_hit(ray, 1.0f)); // voxel is past max_t
}

TEST_CASE("render hits the voxel in the center and sees sky in the corner") {
    voxel::MaterialPalette palette;
    const TriangleScene scene = single_voxel_scene(palette);

    RenderSettings settings;
    settings.width = 17;
    settings.height = 17;
    settings.shadows = false;

    PinholeCamera cam({4.5f, 4.5f, 12.0f}, {0, 0, -1}, {0, 1, 0}, 0.5f, 1.0f);
    const auto fb = pathtracer::render::render(scene, cam, palette, settings);
    REQUIRE(fb.size() == 17u * 17u);

    const auto &center = fb[8 * 17 + 8];
    const auto &corner = fb[0];

    // Corner looks at empty space -> background sky color.
    CHECK(corner.x == doctest::Approx(settings.background.x));
    CHECK(corner.z == doctest::Approx(settings.background.z));
    // Center hit the gray voxel -> different from the (blue-ish) sky.
    const bool differs = std::abs(center.x - settings.background.x) > 1e-3f
            || std::abs(center.z - settings.background.z) > 1e-3f;
    CHECK(differs);
}

TEST_CASE("emissive material adds brightness at the hit") {
    voxel::MaterialPalette palette;
    voxel::PbrMaterial lamp;
    lamp.albedo = {0.0f, 0.0f, 0.0f};
    lamp.emission = {3.0f, 3.0f, 3.0f};
    palette.add(lamp); // id 1

    voxel::VoxelChunk chunk;
    chunk.set(4, 4, 4, 1);
    TriangleScene scene;
    scene.add_mesh(voxel::greedy_mesh(chunk));

    RenderSettings settings;
    settings.width = 9;
    settings.height = 9;
    PinholeCamera cam({4.5f, 4.5f, 12.0f}, {0, 0, -1}, {0, 1, 0}, 0.4f, 1.0f);
    const auto fb = pathtracer::render::render(scene, cam, palette, settings);

    const auto &center = fb[4 * 9 + 4];
    // Pure-black albedo + emission 3 => center is emission-dominated and bright.
    CHECK(center.x == doctest::Approx(3.0f));
}
