#include "doctest.h"

#include <vector>

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

// A diffuse floor lit by an emissive slab above it; nothing else emits.
TriangleScene lit_room(voxel::MaterialPalette &palette) {
    voxel::PbrMaterial floor;
    floor.albedo = {0.7f, 0.7f, 0.7f};
    const auto floor_id = palette.add(floor);

    voxel::PbrMaterial light;
    light.albedo = {0.0f, 0.0f, 0.0f};
    light.emission = {6.0f, 6.0f, 6.0f};
    const auto light_id = palette.add(light);

    voxel::VoxelChunk chunk;
    box(chunk, 0, 0, 0, 7, 0, 7, floor_id);  // floor
    box(chunk, 2, 6, 2, 5, 6, 5, light_id);  // emissive ceiling slab

    TriangleScene scene;
    scene.add_mesh(voxel::greedy_mesh(chunk));
    scene.build_bvh();
    scene.build_lights(palette);
    return scene;
}

PinholeCamera room_camera() {
    return PinholeCamera({4.0f, 7.0f, 15.0f}, core::Vec3{4, 0, 4} - core::Vec3{4, 7, 15},
                         {0, 1, 0}, 0.6f, 1.0f);
}

double mean_x(const std::vector<core::Vec3> &fb) {
    double s = 0.0;
    for (const auto &c : fb) s += c.x;
    return s / fb.size();
}

double variance_x(const std::vector<core::Vec3> &fb) {
    const double m = mean_x(fb);
    double s = 0.0;
    for (const auto &c : fb) s += (c.x - m) * (c.x - m);
    return s / fb.size();
}

} // namespace

TEST_CASE("NEE: the area light is collected and sampled") {
    voxel::MaterialPalette palette;
    const TriangleScene scene = lit_room(palette);
    REQUIRE(scene.has_lights());
    CHECK(scene.total_emissive_area() > 0.0f);

    // A sampled light point lies on the emissive slab (y == 6 top or 7 if a +y face).
    const auto ls = scene.sample_light(0.5f, 0.3f, 0.7f);
    CHECK(ls.emission.x == doctest::Approx(6.0f));
    CHECK(ls.pdf_area == doctest::Approx(1.0f / scene.total_emissive_area()));
}

TEST_CASE("NEE+MIS matches the BSDF-only path tracer in expectation") {
    voxel::MaterialPalette palette;
    const TriangleScene scene = lit_room(palette);

    PathSettings base;
    base.width = 48;
    base.height = 48;
    base.spp = 160;
    base.max_depth = 6;
    base.env_radiance = {0.0f, 0.0f, 0.0f}; // only the area light contributes

    PathSettings nee = base;
    nee.next_event_estimation = true;

    const auto ref = path_render(scene, room_camera(), palette, base, 2024);
    const auto with_nee = path_render(scene, room_camera(), palette, nee, 7);

    // Both estimators are unbiased for the same integral; their image means agree.
    const double m_ref = mean_x(ref);
    const double m_nee = mean_x(with_nee);
    CHECK(m_ref > 0.05); // sanity: the scene is actually lit
    CHECK(m_nee == doctest::Approx(m_ref).epsilon(0.05));
}

TEST_CASE("NEE reduces variance versus BSDF sampling at equal spp") {
    voxel::MaterialPalette palette;
    const TriangleScene scene = lit_room(palette);

    PathSettings base;
    base.width = 48;
    base.height = 48;
    base.spp = 8; // low spp exposes the noise difference
    base.max_depth = 6;
    base.env_radiance = {0.0f, 0.0f, 0.0f};

    PathSettings nee = base;
    nee.next_event_estimation = true;

    const auto ref = path_render(scene, room_camera(), palette, base, 1);
    const auto with_nee = path_render(scene, room_camera(), palette, nee, 1);

    CHECK(variance_x(with_nee) < variance_x(ref));
}
