#include "render/cpu_reference.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

namespace pathtracer::render {

using core::Ray;
using core::Triangle;
using core::Vec3;

void TriangleScene::add_mesh(const voxel::MeshData &mesh) {
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const voxel::Vertex &a = mesh.vertices[mesh.indices[i]];
        const voxel::Vertex &b = mesh.vertices[mesh.indices[i + 1]];
        const voxel::Vertex &c = mesh.vertices[mesh.indices[i + 2]];
        triangles_.push_back(Triangle{{a.px, a.py, a.pz}, {b.px, b.py, b.pz}, {c.px, c.py, c.pz}});
        // Greedy-mesh faces are flat, so any vertex normal represents the triangle.
        normals_.push_back(Vec3{a.nx, a.ny, a.nz});
        materials_.push_back(a.material);
    }
}

void TriangleScene::build_bvh() {
    bvh_.build(triangles_);
    use_bvh_ = true;
}

Hit TriangleScene::make_hit(int triangle_index, float t) const {
    Hit hit;
    hit.hit = true;
    hit.t = t;
    hit.normal = normals_[triangle_index];
    hit.material = materials_[triangle_index];
    return hit;
}

Hit TriangleScene::closest_hit(const Ray &ray) const {
    if (use_bvh_) {
        const Bvh::Hit h = bvh_.closest(ray);
        return h.hit ? make_hit(h.index, h.t) : Hit{};
    }
    return brute_force_closest_hit(ray);
}

bool TriangleScene::any_hit(const Ray &ray, float max_t) const {
    return use_bvh_ ? bvh_.any(ray, max_t) : brute_force_any_hit(ray, max_t);
}

Hit TriangleScene::brute_force_closest_hit(const Ray &ray) const {
    Hit best;
    float closest = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < triangles_.size(); ++i) {
        const auto t = core::intersect(ray, triangles_[i]);
        if (t && *t < closest) {
            closest = *t;
            best = make_hit(static_cast<int>(i), *t);
        }
    }
    return best;
}

bool TriangleScene::brute_force_any_hit(const Ray &ray, float max_t) const {
    for (const Triangle &tri : triangles_) {
        const auto t = core::intersect(ray, tri);
        if (t && *t < max_t) {
            return true;
        }
    }
    return false;
}

std::vector<Vec3> render(const TriangleScene &scene, const PinholeCamera &camera,
                         const voxel::MaterialPalette &palette, const RenderSettings &settings) {
    std::vector<Vec3> fb(static_cast<std::size_t>(settings.width) * settings.height);

    const Vec3 to_sun = core::normalize(-settings.sun_dir);

    for (int y = 0; y < settings.height; ++y) {
        for (int x = 0; x < settings.width; ++x) {
            // Screen coords with (0,0) bottom-left so the image is right-side up.
            const float sx = (static_cast<float>(x) + 0.5f) / static_cast<float>(settings.width);
            const float sy = 1.0f - (static_cast<float>(y) + 0.5f) / static_cast<float>(settings.height);
            const Ray ray = camera.generate_ray(sx, sy);

            const Hit hit = scene.closest_hit(ray);
            Vec3 color;
            if (!hit.hit) {
                color = settings.background;
            } else {
                const voxel::PbrMaterial &mat =
                        palette.contains(hit.material) ? palette.get(hit.material)
                                                       : voxel::PbrMaterial{};
                const Vec3 albedo{mat.albedo[0], mat.albedo[1], mat.albedo[2]};
                const Vec3 emission{mat.emission[0], mat.emission[1], mat.emission[2]};

                float n_dot_l = std::max(0.0f, core::dot(hit.normal, to_sun));
                if (n_dot_l > 0.0f && settings.shadows) {
                    const Vec3 p = ray.origin + ray.dir * hit.t;
                    const Ray shadow{p + hit.normal * 1e-3f, to_sun};
                    if (scene.any_hit(shadow, 1e30f)) {
                        n_dot_l = 0.0f;
                    }
                }

                const Vec3 lit{
                        settings.ambient.x + settings.sun_color.x * n_dot_l,
                        settings.ambient.y + settings.sun_color.y * n_dot_l,
                        settings.ambient.z + settings.sun_color.z * n_dot_l};
                color = Vec3{emission.x + albedo.x * lit.x, emission.y + albedo.y * lit.y,
                             emission.z + albedo.z * lit.z};
            }
            fb[static_cast<std::size_t>(y) * settings.width + x] = color;
        }
    }
    return fb;
}

bool write_ppm(const std::string &path, const std::vector<Vec3> &framebuffer, int width,
               int height) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << "P6\n" << width << ' ' << height << "\n255\n";
    auto encode = [](float c) -> char {
        const float g = std::pow(std::clamp(c, 0.0f, 1.0f), 1.0f / 2.2f);
        return static_cast<char>(std::lround(g * 255.0f));
    };
    for (const Vec3 &c : framebuffer) {
        const char rgb[3] = {encode(c.x), encode(c.y), encode(c.z)};
        out.write(rgb, 3);
    }
    return static_cast<bool>(out);
}

} // namespace pathtracer::render
