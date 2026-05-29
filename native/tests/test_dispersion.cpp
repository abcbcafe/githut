#include "doctest.h"

#include <cmath>

#include "core/math.h"
#include "render/bsdf.h"
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
namespace bsdf = pathtracer::render::bsdf;

// ior_for_channel lives in an anonymous namespace inside path_integrator.cpp, so we
// reproduce the Cauchy relation here to test the same physics the integrator uses.
namespace {
float cauchy_ior(float ior_green, float dispersion, int channel) {
    const float lambda[3] = {0.620f, 0.540f, 0.460f};
    const float a = ior_green - dispersion / (lambda[1] * lambda[1]);
    return a + dispersion / (lambda[channel] * lambda[channel]);
}

void box(voxel::VoxelChunk &c, int x0, int y0, int z0, int x1, int y1, int z1,
         voxel::MaterialId m) {
    for (int z = z0; z <= z1; ++z)
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) c.set(x, y, z, m);
}

// Total chromatic separation in an image: how far each pixel is from gray.
double chroma_sum(const std::vector<core::Vec3> &fb) {
    double s = 0.0;
    for (const auto &c : fb) s += std::fabs(c.x - c.z) + std::fabs(c.x - c.y);
    return s;
}
} // namespace

TEST_CASE("Cauchy IOR: blue refracts more than red, green is the anchor") {
    const float green = 1.5f;
    // No dispersion => achromatic.
    CHECK(cauchy_ior(green, 0.0f, 0) == doctest::Approx(green));
    CHECK(cauchy_ior(green, 0.0f, 2) == doctest::Approx(green));

    // Normal dispersion: n_blue > n_green > n_red, and green stays at the anchor.
    const float nr = cauchy_ior(green, 0.004f, 0);
    const float ng = cauchy_ior(green, 0.004f, 1);
    const float nb = cauchy_ior(green, 0.004f, 2);
    CHECK(ng == doctest::Approx(green));
    CHECK(nb > ng);
    CHECK(ng > nr);
}

TEST_CASE("higher IOR (blue) bends the refracted ray more toward the normal") {
    const core::Vec3 n{0.0f, 0.0f, 1.0f};
    const float theta_i = 0.6f;
    const core::Vec3 d{std::sin(theta_i), 0.0f, -std::cos(theta_i)};

    const float nr = cauchy_ior(1.5f, 0.004f, 0);
    const float nb = cauchy_ior(1.5f, 0.004f, 2);

    core::Vec3 wr, wb;
    REQUIRE(bsdf::refract(d, n, 1.0f / nr, wr)); // air -> glass, red
    REQUIRE(bsdf::refract(d, n, 1.0f / nb, wb)); // air -> glass, blue

    // Transmitted tangential component = sin(theta_t); smaller means bent more.
    const float sin_t_red = std::sqrt(wr.x * wr.x + wr.y * wr.y);
    const float sin_t_blue = std::sqrt(wb.x * wb.x + wb.y * wb.y);
    CHECK(sin_t_blue < sin_t_red); // blue bends more
}

TEST_CASE("dispersion introduces chromatic separation that is absent without it") {
    auto build = [](voxel::MaterialPalette &palette, float dispersion) {
        voxel::PbrMaterial wall;
        wall.emission = {1.0f, 1.0f, 1.0f}; // white light source
        const auto wall_id = palette.add(wall);
        voxel::PbrMaterial glass;
        glass.transmission = 1.0f;
        glass.ior = 1.5f;
        glass.roughness = 0.0f;
        glass.dispersion = dispersion;
        const auto glass_id = palette.add(glass);
        voxel::VoxelChunk chunk;
        box(chunk, 0, 0, 0, 11, 11, 0, wall_id);
        box(chunk, 3, 3, 5, 8, 8, 5, glass_id);
        TriangleScene scene;
        scene.add_mesh(voxel::greedy_mesh(chunk));
        scene.build_bvh();
        return scene;
    };

    // Wide field of view so off-axis rays strike the slab obliquely (where a prism
    // actually splits colors).
    PinholeCamera cam({5.5f, 5.5f, 11.0f}, {0, 0, -1}, {0, 1, 0}, 0.9f, 1.0f);
    PathSettings s;
    s.width = 64;
    s.height = 64;
    s.spp = 200;
    s.max_depth = 8;
    s.env_radiance = {0.0f, 0.0f, 0.0f};

    voxel::MaterialPalette p_clear, p_disp;
    const auto clear = build(p_clear, 0.0f);
    const auto disp = build(p_disp, 0.02f);

    const double chroma_clear = chroma_sum(path_render(clear, cam, p_clear, s, 1));
    const double chroma_disp = chroma_sum(path_render(disp, cam, p_disp, s, 1));

    // Clear glass + white light + gray scene => essentially achromatic.
    // Dispersive glass splits colors => much larger chromatic separation.
    CHECK(chroma_disp > chroma_clear * 5.0 + 1.0);
}

TEST_CASE("frosted + prismatic: rough dispersive glass both blurs and splits colors") {
    auto build = [](voxel::MaterialPalette &palette, float dispersion) {
        voxel::PbrMaterial wall;
        wall.emission = {1.0f, 1.0f, 1.0f};
        const auto wall_id = palette.add(wall);
        voxel::PbrMaterial glass;
        glass.transmission = 1.0f;
        glass.ior = 1.5f;
        glass.roughness = 0.3f; // frosted
        glass.dispersion = dispersion;
        const auto glass_id = palette.add(glass);
        voxel::VoxelChunk chunk;
        box(chunk, 0, 0, 0, 11, 11, 0, wall_id);
        box(chunk, 3, 3, 5, 8, 8, 5, glass_id);
        TriangleScene scene;
        scene.add_mesh(voxel::greedy_mesh(chunk));
        scene.build_bvh();
        return scene;
    };

    PinholeCamera cam({5.5f, 5.5f, 11.0f}, {0, 0, -1}, {0, 1, 0}, 0.9f, 1.0f);
    PathSettings s;
    s.width = 64;
    s.height = 64;
    s.spp = 256;
    s.max_depth = 8;
    s.env_radiance = {0.0f, 0.0f, 0.0f};

    voxel::MaterialPalette p0, p1;
    const double chroma_plain = chroma_sum(path_render(build(p0, 0.0f), cam, p0, s, 1));
    const double chroma_disp = chroma_sum(path_render(build(p1, 0.02f), cam, p1, s, 1));
    // Frosted glass with dispersion still separates colors well beyond frosted-only.
    CHECK(chroma_disp > chroma_plain * 2.0 + 1.0);
}

TEST_CASE("dispersive glass still conserves energy (furnace bound)") {
    voxel::MaterialPalette palette;
    voxel::PbrMaterial glass;
    glass.transmission = 1.0f;
    glass.ior = 1.5f;
    glass.dispersion = 0.02f;
    palette.add(glass); // id 1
    voxel::VoxelChunk chunk;
    for (int z = 4; z <= 5; ++z)
        for (int y = 4; y <= 5; ++y)
            for (int x = 4; x <= 5; ++x) chunk.set(x, y, z, 1);
    TriangleScene scene;
    scene.add_mesh(voxel::greedy_mesh(chunk));
    scene.build_bvh();

    PathSettings s;
    s.width = 21;
    s.height = 21;
    s.spp = 600; // hero-wavelength selection adds variance, so use more samples
    s.max_depth = 24;
    s.env_radiance = {0.5f, 0.5f, 0.5f};
    const auto fb = path_render(scene, PinholeCamera({4.5f, 4.5f, 12.0f}, {0, 0, -1}, {0, 1, 0},
                                                     0.4f, 1.0f),
                                palette, s, 2);
    double sum = 0.0;
    for (const auto &c : fb) sum += c.x;
    CHECK(sum / fb.size() <= 0.5 + 0.02); // never brighter than the white furnace
}
