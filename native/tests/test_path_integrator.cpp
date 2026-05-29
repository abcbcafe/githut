#include "doctest.h"

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

// A single convex voxel of the given albedo, framed by a close-up camera.
TriangleScene single_voxel(voxel::MaterialPalette &palette, std::array<float, 3> albedo) {
    voxel::PbrMaterial mat;
    mat.albedo = albedo;
    palette.add(mat); // id 1
    voxel::VoxelChunk chunk;
    chunk.set(4, 4, 4, 1);
    TriangleScene scene;
    scene.add_mesh(voxel::greedy_mesh(chunk));
    scene.build_bvh();
    return scene;
}

PinholeCamera close_camera() {
    return PinholeCamera({4.5f, 4.5f, 9.0f}, {0, 0, -1}, {0, 1, 0}, 0.3f, 1.0f);
}

} // namespace

TEST_CASE("path render is deterministic for a fixed seed") {
    voxel::MaterialPalette palette;
    const TriangleScene scene = single_voxel(palette, {0.7f, 0.7f, 0.7f});
    PathSettings s;
    s.width = 8;
    s.height = 8;
    s.spp = 4;
    s.env_radiance = {0.5f, 0.5f, 0.5f};

    const auto a = path_render(scene, close_camera(), palette, s, 123);
    const auto b = path_render(scene, close_camera(), palette, s, 123);
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].x == b[i].x);
        CHECK(a[i].y == b[i].y);
        CHECK(a[i].z == b[i].z);
    }
}

// White-furnace test: a convex object in a constant environment E with diffuse
// albedo a should reflect radiance a*E (no energy created or lost). A ray that
// misses returns E directly. These hold near-exactly because the environment is
// constant and a convex voxel bounce always escapes after one bounce.
TEST_CASE("white furnace: albedo 1 conserves energy (surface == environment)") {
    voxel::MaterialPalette palette;
    const TriangleScene scene = single_voxel(palette, {1.0f, 1.0f, 1.0f});
    PathSettings s;
    s.width = 13;
    s.height = 13;
    s.spp = 16;
    s.max_depth = 8;
    s.env_radiance = {0.5f, 0.5f, 0.5f};

    const auto fb = path_render(scene, close_camera(), palette, s, 1);
    const auto &center = fb[6 * 13 + 6];
    const auto &corner = fb[0];

    CHECK(corner.x == doctest::Approx(0.5f).epsilon(1e-3)); // miss => environment
    CHECK(center.x == doctest::Approx(0.5f).epsilon(1e-3)); // a=1 => a*E = E
}

TEST_CASE("white furnace: albedo 0.5 reflects half the environment") {
    voxel::MaterialPalette palette;
    const TriangleScene scene = single_voxel(palette, {0.5f, 0.5f, 0.5f});
    PathSettings s;
    s.width = 13;
    s.height = 13;
    s.spp = 16;
    s.max_depth = 8;
    s.env_radiance = {0.5f, 0.5f, 0.5f};

    const auto fb = path_render(scene, close_camera(), palette, s, 1);
    const auto &center = fb[6 * 13 + 6];
    const auto &corner = fb[0];

    CHECK(corner.x == doctest::Approx(0.5f).epsilon(1e-3));  // miss => E
    CHECK(center.x == doctest::Approx(0.25f).epsilon(1e-3)); // a*E = 0.5*0.5
}

namespace {
// A single emissive voxel (albedo 0) framed head-on at distance 4 from the camera.
TriangleScene emissive_voxel(voxel::MaterialPalette &palette, float emission) {
    voxel::PbrMaterial lamp;
    lamp.albedo = {0.0f, 0.0f, 0.0f};
    lamp.emission = {emission, emission, emission};
    palette.add(lamp); // id 1
    voxel::VoxelChunk chunk;
    chunk.set(4, 4, 4, 1);
    TriangleScene scene;
    scene.add_mesh(voxel::greedy_mesh(chunk));
    scene.build_bvh();
    return scene;
}
} // namespace

TEST_CASE("a vacuum medium leaves the image unchanged") {
    voxel::MaterialPalette palette;
    const TriangleScene scene = single_voxel(palette, {0.7f, 0.7f, 0.7f});
    PathSettings base;
    base.width = 8;
    base.height = 8;
    base.spp = 4;
    base.env_radiance = {0.5f, 0.5f, 0.5f};

    PathSettings fogged = base;
    fogged.medium_enabled = true; // sigma == 0 => transmittance 1 => no change

    const auto a = path_render(scene, close_camera(), palette, base, 7);
    const auto b = path_render(scene, close_camera(), palette, fogged, 7);
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].x == doctest::Approx(b[i].x));
    }
}

TEST_CASE("absorption fog attenuates an emitter by Beer-Lambert exp(-sigma_t*d)") {
    voxel::MaterialPalette palette;
    const float emission = 2.0f;
    const TriangleScene scene = emissive_voxel(palette, emission);

    PathSettings s;
    s.width = 9;
    s.height = 9;
    s.spp = 4;
    s.env_radiance = {0.0f, 0.0f, 0.0f};
    s.medium_enabled = true;
    s.medium.sigma_a = {0.15f, 0.15f, 0.15f}; // absorption only
    s.fog_inscatter = {0.0f, 0.0f, 0.0f};      // no in-scatter for a pure absorber

    const auto fb = path_render(scene, close_camera(), palette, s, 1);
    // Camera at z=9 looking -z hits the front face at z=5 => distance 4.
    const float expected = emission * std::exp(-0.15f * 4.0f);
    CHECK(fb[4 * 9 + 4].x == doctest::Approx(expected).epsilon(2e-2));
}

TEST_CASE("thick fog blends a surface toward the in-scatter color") {
    voxel::MaterialPalette palette;
    const TriangleScene scene = emissive_voxel(palette, 2.0f);

    PathSettings s;
    s.width = 9;
    s.height = 9;
    s.spp = 4;
    s.medium_enabled = true;
    s.medium.sigma_a = {5.0f, 5.0f, 5.0f}; // very thick => T ~ 0
    s.fog_inscatter = {0.3f, 0.4f, 0.5f};

    const auto fb = path_render(scene, close_camera(), palette, s, 1);
    CHECK(fb[4 * 9 + 4].x == doctest::Approx(0.3f).epsilon(1e-2));
    CHECK(fb[4 * 9 + 4].z == doctest::Approx(0.5f).epsilon(1e-2));
}

TEST_CASE("emissive surface is at least as bright as its emission") {
    voxel::MaterialPalette palette;
    voxel::PbrMaterial lamp;
    lamp.albedo = {0.0f, 0.0f, 0.0f};
    lamp.emission = {2.0f, 2.0f, 2.0f};
    palette.add(lamp); // id 1
    voxel::VoxelChunk chunk;
    chunk.set(4, 4, 4, 1);
    TriangleScene scene;
    scene.add_mesh(voxel::greedy_mesh(chunk));
    scene.build_bvh();

    PathSettings s;
    s.width = 9;
    s.height = 9;
    s.spp = 4;
    s.env_radiance = {0.0f, 0.0f, 0.0f};
    const auto fb = path_render(scene, close_camera(), palette, s, 5);
    CHECK(fb[4 * 9 + 4].x == doctest::Approx(2.0f)); // pure emitter, no albedo
}
