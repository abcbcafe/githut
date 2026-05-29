#include "render/path_integrator.h"

#include <algorithm>

namespace pathtracer::render {

using core::Pcg32;
using core::Ray;
using core::Vec3;

namespace {
// Component-wise (Hadamard) product.
Vec3 mul(const Vec3 &a, const Vec3 &b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }

Vec3 mat_albedo(const voxel::PbrMaterial &m) { return {m.albedo[0], m.albedo[1], m.albedo[2]}; }
Vec3 mat_emission(const voxel::PbrMaterial &m) {
    return {m.emission[0], m.emission[1], m.emission[2]};
}
} // namespace

Vec3 trace_path(const TriangleScene &scene, const voxel::MaterialPalette &palette, Ray ray,
                Pcg32 &rng, int max_depth, const Vec3 &env_radiance) {
    Vec3 radiance{0.0f, 0.0f, 0.0f};
    Vec3 beta{1.0f, 1.0f, 1.0f}; // path throughput

    for (int depth = 0; depth < max_depth; ++depth) {
        const Hit hit = scene.closest_hit(ray);
        if (!hit.hit) {
            radiance = radiance + mul(beta, env_radiance); // escaped to the environment
            break;
        }

        const voxel::PbrMaterial &mat =
                palette.contains(hit.material) ? palette.get(hit.material) : voxel::PbrMaterial{};

        radiance = radiance + mul(beta, mat_emission(mat));

        // Shading normal oriented against the incoming ray (two-sided surfaces).
        Vec3 n = hit.normal;
        if (core::dot(n, ray.dir) > 0.0f) {
            n = -n;
        }

        // Lambertian bounce with cosine-importance sampling: f*cos/pdf = albedo.
        beta = mul(beta, mat_albedo(mat));

        // Russian roulette once throughput can have shrunk.
        if (depth >= 3) {
            const float q = std::clamp(std::max({beta.x, beta.y, beta.z}), 0.0f, 0.95f);
            if (rng.next_float() > q) {
                break;
            }
            beta = beta * (1.0f / q);
        }

        const Vec3 local = core::cosine_sample_hemisphere(rng.next_float(), rng.next_float());
        const Vec3 dir = core::normalize(core::to_world(local, n));
        const Vec3 p = ray.origin + ray.dir * hit.t;
        ray = Ray{p + n * 1e-3f, dir};
    }

    return radiance;
}

std::vector<Vec3> path_render(const TriangleScene &scene, const PinholeCamera &camera,
                              const voxel::MaterialPalette &palette, const PathSettings &settings,
                              uint64_t seed) {
    std::vector<Vec3> fb(static_cast<std::size_t>(settings.width) * settings.height);

    for (int y = 0; y < settings.height; ++y) {
        for (int x = 0; x < settings.width; ++x) {
            const std::size_t pixel = static_cast<std::size_t>(y) * settings.width + x;
            // Per-pixel stream keyed by seed + pixel index => deterministic.
            Pcg32 rng(seed, pixel * 2u + 1u);

            Vec3 sum{0.0f, 0.0f, 0.0f};
            for (int s = 0; s < settings.spp; ++s) {
                const float jx = rng.next_float();
                const float jy = rng.next_float();
                const float sx = (static_cast<float>(x) + jx) / static_cast<float>(settings.width);
                const float sy =
                        1.0f - (static_cast<float>(y) + jy) / static_cast<float>(settings.height);
                const Ray ray = camera.generate_ray(sx, sy);
                sum = sum + trace_path(scene, palette, ray, rng, settings.max_depth,
                                       settings.env_radiance);
            }
            const float inv = 1.0f / static_cast<float>(settings.spp);
            fb[pixel] = sum * inv;
        }
    }
    return fb;
}

} // namespace pathtracer::render
