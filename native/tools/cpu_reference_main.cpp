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
#include "render/camera.h"
#include "render/cpu_reference.h"
#include "render/path_integrator.h"
#include "voxel/greedy_mesher.h"
#include "voxel/material_palette.h"
#include "voxel/vox_loader.h"

using namespace pathtracer;

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

void build_demo(voxel::VoxelChunk &chunk, voxel::MaterialPalette &palette, bool glass_block) {
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
        blue.attenuation = {0.05f, 0.02f, 0.08f}; // faint tint
    }
    const auto blue_id = palette.add(blue);

    voxel::PbrMaterial lamp;
    lamp.albedo = {0.0f, 0.0f, 0.0f};
    lamp.emission = {2.4f, 2.2f, 1.8f};
    const auto lamp_id = palette.add(lamp);

    box(chunk, 0, 0, 0, 21, 0, 21, floor_id); // floor slab
    box(chunk, 3, 1, 3, 4, 6, 4, red_id);     // red pillar
    box(chunk, 16, 1, 5, 17, 8, 6, green_id); // green pillar
    box(chunk, 9, 1, 14, 12, 4, 17, blue_id); // blue block
    box(chunk, 8, 7, 8, 9, 8, 9, lamp_id);    // emissive block
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
    int spp_override = -1;
    const char *vox_path = nullptr;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--pt") {
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
        } else if (arg.rfind("--spp=", 0) == 0) {
            spp_override = std::atoi(arg.c_str() + 6);
        } else {
            vox_path = argv[i];
        }
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
        build_demo(chunk, palette, glass);
        std::printf("rendering built-in demo scene%s\n", glass ? " (glass block)" : "");
    }

    const voxel::MeshData mesh = voxel::greedy_mesh(chunk);
    render::TriangleScene scene;
    scene.add_mesh(mesh);
    scene.build_bvh();
    scene.build_lights(palette);
    std::printf("greedy mesh: %u quads, %zu triangles, %s emissive lights\n", mesh.quad_count,
                scene.triangle_count(), scene.has_lights() ? "has" : "no");

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
