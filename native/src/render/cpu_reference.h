#pragma once

// A small, GPU-free reference path tracer. Its job is not speed but ground truth:
// it renders the same scene data (greedy-meshed voxels + material palette) the GPU
// path tracer will, so its output serves as the golden image for verification (see
// docs/ROADMAP.md) and lets the shading/intersection math be unit-tested on CPU.

#include <cstdint>
#include <string>
#include <vector>

#include "core/math.h"
#include "render/camera.h"
#include "voxel/material_palette.h"
#include "voxel/mesh_data.h"

namespace pathtracer::render {

struct Hit {
    bool hit = false;
    float t = 0.0f;
    core::Vec3 normal;
    voxel::MaterialId material = voxel::kAir;
};

// A flat triangle soup with per-triangle normal + material, built from greedy mesh
// output. Brute-force intersection is fine for coarse voxel scenes.
class TriangleScene {
public:
    void add_mesh(const voxel::MeshData &mesh);
    Hit closest_hit(const core::Ray &ray) const;
    bool any_hit(const core::Ray &ray, float max_t) const; // shadow rays
    std::size_t triangle_count() const { return triangles_.size(); }

private:
    std::vector<core::Triangle> triangles_;
    std::vector<core::Vec3> normals_;
    std::vector<voxel::MaterialId> materials_;
};

struct RenderSettings {
    int width = 320;
    int height = 180;
    core::Vec3 sun_dir{0.4f, -1.0f, -0.3f}; // direction the sunlight travels
    core::Vec3 sun_color{1.0f, 0.96f, 0.9f};
    core::Vec3 ambient{0.12f, 0.13f, 0.16f};
    core::Vec3 background{0.55f, 0.7f, 0.95f}; // sky
    bool shadows = true;
};

// Render to a linear RGB framebuffer (row-major, size = width*height).
std::vector<core::Vec3> render(const TriangleScene &scene, const PinholeCamera &camera,
                               const voxel::MaterialPalette &palette,
                               const RenderSettings &settings);

// Write a framebuffer to a binary PPM (P6), applying a simple gamma 2.2 + clamp.
bool write_ppm(const std::string &path, const std::vector<core::Vec3> &framebuffer, int width,
               int height);

} // namespace pathtracer::render
