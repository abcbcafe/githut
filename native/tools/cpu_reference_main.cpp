// Offline CPU reference renderer CLI.
//
//   cpu_reference <out.ppm> [scene.vox]
//
// With no .vox file it renders a built-in demo (floor + colored pillars + an
// emissive block). With a .vox file it imports and frames that model. The output
// PPM is the "golden image" used to sanity-check the GPU path tracer.

#include <algorithm>
#include <cstdio>
#include <string>

#include "core/math.h"
#include "render/caustics.h"
#include "render/camera.h"
#include "render/cpu_reference.h"
#include "render/path_integrator.h"
#include "voxel/greedy_mesher.h"
#include "voxel/material_palette.h"
#include "voxel/vox_loader.h"

using namespace pathtracer;

// Dedicated caustic showcase: a clear glass sphere over a diffuse floor, lit by an
// overhead panel. Photons are traced through the sphere to build a floor caustic map,
// then the camera pass reads it. Returns 0 on success.
static int render_caustics(const std::string &out_path, int spp) {
    voxel::MaterialPalette palette;
    voxel::PbrMaterial floor;
    floor.albedo = {0.80f, 0.80f, 0.82f};
    const auto floor_id = palette.add(floor);
    voxel::PbrMaterial light;
    light.emission = {60.0f, 58.0f, 52.0f};
    const auto light_id = palette.add(light);
    voxel::PbrMaterial glass;
    glass.transmission = 1.0f;
    glass.ior = 1.5f;
    glass.roughness = 0.0f;
    const auto glass_id = palette.add(glass);

    voxel::VoxelChunk chunk;
    for (int z = 0; z < 24; ++z)
        for (int x = 0; x < 24; ++x) chunk.set(x, 0, z, floor_id); // floor, top at y=1
    for (int z = 9; z < 15; ++z)
        for (int x = 9; x < 15; ++x) chunk.set(x, 22, z, light_id); // overhead panel

    render::TriangleScene scene;
    scene.add_mesh(voxel::greedy_mesh(chunk));
    scene.build_bvh();
    scene.build_lights(palette);
    scene.add_sphere({12.0f, 6.0f, 12.0f}, 3.5f, glass_id);

    render::CausticMap caustics(0.0f, 0.0f, 24.0f, 24.0f, 320, 1.0f);
    const int photons = 6'000'000;
    std::printf("tracing %d caustic photons...\n", photons);
    render::trace_caustics(scene, palette, caustics, photons, 1);

    render::PathSettings ps;
    ps.width = 640;
    ps.height = 360;
    ps.spp = spp > 0 ? spp : 256;
    ps.max_depth = 6;
    ps.env_radiance = {0.05f, 0.06f, 0.08f};
    ps.next_event_estimation = true;
    ps.caustic = &caustics;

    const float aspect = static_cast<float>(ps.width) / ps.height;
    render::PinholeCamera cam({12.0f, 13.0f, 30.0f}, core::Vec3{12, 1, 11} - core::Vec3{12, 13, 30},
                             {0, 1, 0}, 0.7f, aspect);
    std::printf("path tracing caustic scene: %d spp\n", ps.spp);
    const auto fb = render::path_render(scene, cam, palette, ps, 1);
    if (!render::write_ppm(out_path, fb, ps.width, ps.height)) {
        std::fprintf(stderr, "failed to write %s\n", out_path.c_str());
        return 1;
    }
    std::printf("wrote %s (%dx%d)\n", out_path.c_str(), ps.width, ps.height);
    return 0;
}

namespace {

void box(voxel::VoxelChunk &chunk, int x0, int y0, int z0, int x1, int y1, int z1,
         voxel::MaterialId m) {
    for (int z = z0; z <= z1; ++z) {
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                chunk.set(x, y, z, m);
            }
        }
    }
}

void build_demo(voxel::VoxelChunk &chunk, voxel::MaterialPalette &palette, bool glass_block,
                bool dispersion) {
    voxel::PbrMaterial floor;
    floor.albedo = {0.62f, 0.62f, 0.66f};
    const auto floor_id = palette.add(floor);

    voxel::PbrMaterial red;
    red.albedo = {0.82f, 0.16f, 0.16f};
    const auto red_id = palette.add(red);

    voxel::PbrMaterial green;
    green.albedo = {0.20f, 0.70f, 0.26f};
    const auto green_id = palette.add(green);

    voxel::PbrMaterial blue;
    blue.albedo = {0.20f, 0.36f, 0.86f};
    if (glass_block) {
        blue.albedo = {0.0f, 0.0f, 0.0f};
        blue.transmission = 1.0f;
        blue.ior = 1.5f;
        blue.roughness = 0.0f; // smooth, clear glass
        blue.attenuation = {0.05f, 0.02f, 0.08f}; // faint tint
        if (dispersion) {
            blue.attenuation = {0.0f, 0.0f, 0.0f}; // clear, so colors come only from dispersion
            blue.dispersion = 0.05f;               // strong, prism-like
        }
    }
    const auto blue_id = palette.add(blue);

    voxel::PbrMaterial lamp;
    lamp.albedo = {0.0f, 0.0f, 0.0f};
    lamp.emission = {2.4f, 2.2f, 1.8f};
    const auto lamp_id = palette.add(lamp);

    voxel::PbrMaterial wall;
    wall.albedo = {0.72f, 0.69f, 0.60f}; // warm back wall
    const auto wall_id = palette.add(wall);

    voxel::PbrMaterial yellow;
    yellow.albedo = {0.85f, 0.72f, 0.15f};
    const auto yellow_id = palette.add(yellow);

    // A frosted-glass slab (rough dielectric), only shown alongside the glass block.
    voxel::PbrMaterial frosted;
    frosted.transmission = 1.0f;
    frosted.ior = 1.5f;
    frosted.roughness = 0.32f;
    frosted.attenuation = {0.04f, 0.02f, 0.05f};
    const auto frosted_id = palette.add(frosted);

    box(chunk, 0, 0, 0, 21, 0, 21, floor_id); // floor slab
    box(chunk, 0, 1, 0, 21, 7, 0, wall_id);    // back wall (far side)
    box(chunk, 3, 1, 3, 4, 6, 4, red_id);      // red pillar
    box(chunk, 16, 1, 5, 17, 8, 6, green_id);  // green pillar
    box(chunk, 9, 1, 14, 12, 4, 17, blue_id);  // blue block / glass cube
    box(chunk, 17, 1, 15, 19, 3, 17, yellow_id); // yellow box
    box(chunk, 8, 7, 8, 9, 8, 9, lamp_id);     // emissive block
    if (glass_block) {
        box(chunk, 5, 1, 9, 6, 6, 10, frosted_id); // frosted-glass slab
    }
}

// Bounds of solid voxels, used to auto-frame the camera.
void solid_bounds(const voxel::VoxelChunk &chunk, core::Vec3 &min_out, core::Vec3 &max_out) {
    int lo[3] = {voxel::VoxelChunk::kSize, voxel::VoxelChunk::kSize, voxel::VoxelChunk::kSize};
    int hi[3] = {0, 0, 0};
    bool any = false;
    for (int z = 0; z < voxel::VoxelChunk::kSize; ++z) {
        for (int y = 0; y < voxel::VoxelChunk::kSize; ++y) {
            for (int x = 0; x < voxel::VoxelChunk::kSize; ++x) {
                if (chunk.is_solid(x, y, z)) {
                    any = true;
                    lo[0] = std::min(lo[0], x); hi[0] = std::max(hi[0], x);
                    lo[1] = std::min(lo[1], y); hi[1] = std::max(hi[1], y);
                    lo[2] = std::min(lo[2], z); hi[2] = std::max(hi[2], z);
                }
            }
        }
    }
    if (!any) {
        min_out = {0, 0, 0};
        max_out = {1, 1, 1};
        return;
    }
    min_out = {static_cast<float>(lo[0]), static_cast<float>(lo[1]), static_cast<float>(lo[2])};
    max_out = {static_cast<float>(hi[0] + 1), static_cast<float>(hi[1] + 1),
               static_cast<float>(hi[2] + 1)};
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr,
                     "usage: %s <out.ppm> [scene.vox] [--pt]\n"
                     "  --pt   Monte Carlo path tracer (global illumination) instead of\n"
                     "         direct sun + hard shadows\n",
                     argv[0]);
        return 2;
    }
    const std::string out_path = argv[1];

    bool path_trace = false;
    bool fog = false;
    bool nee = false;
    bool glass = false;
    bool dispersion = false;
    bool caustics = false;
    int spp_override = -1;
    const char *vox_path = nullptr;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--caustics") {
            caustics = true;
        } else if (arg == "--pt") {
            path_trace = true;
        } else if (arg == "--fog") {
            fog = true;
            path_trace = true; // fog is applied in the path-traced renderer
        } else if (arg == "--nee") {
            nee = true;
            path_trace = true;
        } else if (arg == "--glass") {
            glass = true;
            path_trace = true; // glass needs the path tracer
        } else if (arg == "--dispersion") {
            glass = true;
            dispersion = true;
            path_trace = true;
        } else if (arg.rfind("--spp=", 0) == 0) {
            spp_override = std::atoi(arg.c_str() + 6);
        } else {
            vox_path = argv[i];
        }
    }

    if (caustics) {
        return render_caustics(out_path, spp_override);
    }

    voxel::VoxelChunk chunk;
    voxel::MaterialPalette palette;

    if (vox_path != nullptr) {
        const auto scene = voxel::load_vox_file(vox_path);
        if (!scene) {
            std::fprintf(stderr, "failed to load .vox: %s\n", vox_path);
            return 1;
        }
        voxel::populate_chunk(*scene, chunk, palette);
        std::printf("loaded %zu voxels from %s\n", scene->voxels.size(), vox_path);
    } else {
        build_demo(chunk, palette, glass, dispersion);
        std::printf("rendering built-in demo scene%s%s\n", glass ? " (glass block)" : "",
                    dispersion ? " (dispersion)" : "");
    }

    const voxel::MeshData mesh = voxel::greedy_mesh(chunk);
    render::TriangleScene scene;
    scene.add_mesh(mesh);
    scene.build_bvh();
    scene.build_lights(palette);
    std::printf("greedy mesh: %u quads, %zu triangles, %s emissive lights\n", mesh.quad_count,
                scene.triangle_count(), scene.has_lights() ? "has" : "no");

    // Two analytic glass spheres for the demo: a smooth clear one and a frosted one
    // (both prismatic when --dispersion is set).
    if (vox_path == nullptr && glass) {
        voxel::PbrMaterial clear_s;
        clear_s.transmission = 1.0f;
        clear_s.ior = 1.5f;
        clear_s.roughness = 0.0f;
        if (dispersion) {
            clear_s.dispersion = 0.05f;
        }
        scene.add_sphere({6.0f, 4.0f, 11.0f}, 2.8f, palette.add(clear_s));

        voxel::PbrMaterial frosted_s;
        frosted_s.transmission = 1.0f;
        frosted_s.ior = 1.5f;
        frosted_s.roughness = 0.28f; // frosted
        if (dispersion) {
            frosted_s.dispersion = 0.05f; // frosted + prismatic
        }
        scene.add_sphere({15.0f, 4.0f, 12.0f}, 2.8f, palette.add(frosted_s));
        std::printf("added 2 glass spheres (clear + frosted%s)\n",
                    dispersion ? ", both prismatic" : "");
    }

    core::Vec3 bmin, bmax;
    solid_bounds(chunk, bmin, bmax);
    const core::Vec3 center{(bmin.x + bmax.x) * 0.5f, (bmin.y + bmax.y) * 0.5f,
                            (bmin.z + bmax.z) * 0.5f};
    const float extent =
            std::max({bmax.x - bmin.x, bmax.y - bmin.y, bmax.z - bmin.z, 1.0f});

    render::RenderSettings settings;
    settings.width = 640;
    settings.height = 360;
    const float aspect = static_cast<float>(settings.width) / settings.height;

    const core::Vec3 eye{center.x, center.y + extent * 0.6f, bmax.z + extent * 1.8f};
    render::PinholeCamera cam(eye, center - eye, {0, 1, 0}, 0.7f, aspect);

    std::vector<core::Vec3> fb;
    if (path_trace) {
        render::PathSettings ps;
        ps.width = settings.width;
        ps.height = settings.height;
        ps.spp = spp_override > 0 ? spp_override : 256;
        ps.max_depth = 6;
        ps.env_radiance = {0.6f, 0.72f, 0.92f}; // sky dome illuminates the scene
        if (fog) {
            ps.medium_enabled = true;
            ps.medium.sigma_s = {0.010f, 0.011f, 0.013f}; // slightly bluer extinction
            ps.medium.g = 0.0f;
            ps.fog_inscatter = {0.62f, 0.74f, 0.94f}; // aerial-perspective haze color
            std::printf("homogeneous fog enabled (aerial perspective)\n");
        }
        ps.next_event_estimation = nee;
        std::printf("path tracing: %d spp, max depth %d%s\n", ps.spp, ps.max_depth,
                    nee ? ", NEE+MIS" : "");
        fb = render::path_render(scene, cam, palette, ps, 1);
    } else {
        fb = render::render(scene, cam, palette, settings);
    }

    if (!render::write_ppm(out_path, fb, settings.width, settings.height)) {
        std::fprintf(stderr, "failed to write %s\n", out_path.c_str());
        return 1;
    }
    std::printf("wrote %s (%dx%d)\n", out_path.c_str(), settings.width, settings.height);
    return 0;
}
