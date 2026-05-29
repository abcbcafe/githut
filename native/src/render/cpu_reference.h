#pragma once

// A small, GPU-free reference path tracer. Its job is not speed but ground truth:
// it renders the same scene data (greedy-meshed voxels + material palette) the GPU
// path tracer will, so its output serves as the golden image for verification (see
// docs/ROADMAP.md) and lets the shading/intersection math be unit-tested on CPU.

#include <cstdint>
#include <string>
#include <vector>

#include "core/math.h"
#include "render/bvh.h"
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

// An emissive triangle treated as a one-sided area light (emits along its normal).
struct AreaLight {
    core::Triangle tri;
    core::Vec3 normal;
    core::Vec3 emission;
    float area = 0.0f;
};

// A point sampled on the emissive surface, for next-event estimation.
struct LightSample {
    core::Vec3 point;
    core::Vec3 normal;
    core::Vec3 emission;
    float pdf_area = 0.0f; // pdf w.r.t. surface area (uniform over total emissive area)
};

// An analytic sphere primitive. Useful for smooth glass spheres in the reference
// renderer (the voxel world itself stays triangle-based).
struct Sphere {
    core::Vec3 center;
    float radius = 1.0f;
    voxel::MaterialId material = voxel::kAir;
};

// A flat triangle soup with per-triangle normal + material, built from greedy mesh
// output, plus optional analytic spheres. Brute-force intersection is fine for
// coarse voxel scenes.
class TriangleScene {
public:
    void add_mesh(const voxel::MeshData &mesh);
    void add_sphere(const core::Vec3 &center, float radius, voxel::MaterialId material);

    // Build the BVH over the current triangles; afterwards closest_hit/any_hit use
    // it. Call once after all meshes are added. Spheres are always brute-forced.
    void build_bvh();

    // Use the BVH when built, otherwise brute force. Results are identical.
    Hit closest_hit(const core::Ray &ray) const;
    bool any_hit(const core::Ray &ray, float max_t) const; // shadow rays

    // Reference implementations, kept for testing the BVH for equivalence.
    Hit brute_force_closest_hit(const core::Ray &ray) const;
    bool brute_force_any_hit(const core::Ray &ray, float max_t) const;

    // Collect emissive triangles into an area-light list (needs the palette for
    // emission). Call after add_mesh(); enables next-event estimation.
    void build_lights(const voxel::MaterialPalette &palette);
    bool has_lights() const { return !lights_.empty(); }
    float total_emissive_area() const { return total_area_; }
    // Sample a point uniformly over the total emissive area (light chosen ∝ area).
    LightSample sample_light(float u_select, float u1, float u2) const;

    std::size_t triangle_count() const { return triangles_.size(); }
    bool has_bvh() const { return use_bvh_; }

private:
    Hit make_hit(int triangle_index, float t) const;
    void intersect_spheres(const core::Ray &ray, Hit &best) const;
    bool any_sphere_hit(const core::Ray &ray, float max_t) const;

    std::vector<core::Triangle> triangles_;
    std::vector<core::Vec3> normals_;
    std::vector<voxel::MaterialId> materials_;

    Bvh bvh_;
    bool use_bvh_ = false;

    std::vector<Sphere> spheres_;

    std::vector<AreaLight> lights_;
    std::vector<float> light_cdf_; // cumulative area, for area-weighted selection
    float total_area_ = 0.0f;
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
