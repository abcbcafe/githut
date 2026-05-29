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
                Pcg32 &rng, int max_depth, const Vec3 &env_radiance, float *out_primary_t) {
    Vec3 radiance{0.0f, 0.0f, 0.0f};
    Vec3 beta{1.0f, 1.0f, 1.0f}; // path throughput

    for (int depth = 0; depth < max_depth; ++depth) {
        const Hit hit = scene.closest_hit(ray);
        if (depth == 0 && out_primary_t != nullptr) {
            *out_primary_t = hit.hit ? hit.t : -1.0f;
        }
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

Vec3 trace_path_nee(const TriangleScene &scene, const voxel::MaterialPalette &palette, Ray ray,
                    Pcg32 &rng, int max_depth, const Vec3 &env_radiance, float *out_primary_t) {
    Vec3 radiance{0.0f, 0.0f, 0.0f};
    Vec3 beta{1.0f, 1.0f, 1.0f};
    const float kInvPi = 1.0f / 3.14159265358979323846f;
    const float area = scene.total_emissive_area();

    bool prev_specular = true; // camera ray has no light-sampling alternative
    float prev_bsdf_pdf = 0.0f;

    for (int depth = 0; depth < max_depth; ++depth) {
        const Hit hit = scene.closest_hit(ray);
        if (depth == 0 && out_primary_t != nullptr) {
            *out_primary_t = hit.hit ? hit.t : -1.0f;
        }
        if (!hit.hit) {
            radiance = radiance + mul(beta, env_radiance);
            break;
        }

        const voxel::PbrMaterial &mat =
                palette.contains(hit.material) ? palette.get(hit.material) : voxel::PbrMaterial{};

        // Emitted radiance, MIS-weighted against the light-sampling strategy.
        const Vec3 emission = mat_emission(mat);
        if (mat.is_emissive()) {
            const float cos_light = core::dot(hit.normal, -ray.dir); // one-sided emitter
            if (cos_light > 0.0f) {
                if (prev_specular || area <= 0.0f) {
                    radiance = radiance + mul(beta, emission);
                } else {
                    const float p_light = (hit.t * hit.t) / (cos_light * area);
                    const float w = prev_bsdf_pdf / (prev_bsdf_pdf + p_light);
                    radiance = radiance + mul(beta, emission) * w;
                }
            }
        }

        Vec3 n = hit.normal;
        if (core::dot(n, ray.dir) > 0.0f) {
            n = -n;
        }
        const Vec3 albedo = mat_albedo(mat);
        const Vec3 p = ray.origin + ray.dir * hit.t;

        // Next-event estimation: sample the emissive surface directly.
        if (area > 0.0f) {
            const LightSample ls =
                    scene.sample_light(rng.next_float(), rng.next_float(), rng.next_float());
            const Vec3 to_light = ls.point - p;
            const float dist2 = core::dot(to_light, to_light);
            const float dist = std::sqrt(dist2);
            if (dist > 1e-4f) {
                const Vec3 wi = to_light * (1.0f / dist);
                const float cos_surf = core::dot(n, wi);
                const float cos_light = core::dot(ls.normal, -wi);
                if (cos_surf > 0.0f && cos_light > 0.0f) {
                    const Ray shadow{p + n * 1e-3f, wi};
                    if (!scene.any_hit(shadow, dist - 2e-3f)) {
                        const float p_light_w = dist2 / (cos_light * area);
                        const float p_bsdf_w = cos_surf * kInvPi; // cosine-weighted diffuse
                        const float w = p_light_w / (p_light_w + p_bsdf_w);
                        const Vec3 f = albedo * kInvPi;
                        radiance = radiance + mul(beta, mul(f, ls.emission)) *
                                                      (cos_surf * w / p_light_w);
                    }
                }
            }
        }

        // BSDF (diffuse) bounce.
        beta = mul(beta, albedo);
        if (depth >= 3) {
            const float q = std::clamp(std::max({beta.x, beta.y, beta.z}), 0.0f, 0.95f);
            if (rng.next_float() > q) {
                break;
            }
            beta = beta * (1.0f / q);
        }
        const Vec3 local = core::cosine_sample_hemisphere(rng.next_float(), rng.next_float());
        const Vec3 dir = core::normalize(core::to_world(local, n));
        prev_bsdf_pdf = local.z * kInvPi; // cosine pdf of the sampled direction
        prev_specular = false;
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

                float primary_t = -1.0f;
                Vec3 L = settings.next_event_estimation
                                 ? trace_path_nee(scene, palette, ray, rng, settings.max_depth,
                                                  settings.env_radiance, &primary_t)
                                 : trace_path(scene, palette, ray, rng, settings.max_depth,
                                              settings.env_radiance, &primary_t);

                // Distance fog / aerial perspective on the primary segment.
                if (settings.medium_enabled && primary_t > 0.0f) {
                    const Vec3 tr = transmittance(settings.medium, primary_t);
                    L = Vec3{L.x * tr.x + settings.fog_inscatter.x * (1.0f - tr.x),
                             L.y * tr.y + settings.fog_inscatter.y * (1.0f - tr.y),
                             L.z * tr.z + settings.fog_inscatter.z * (1.0f - tr.z)};
                }
                sum = sum + L;
            }
            const float inv = 1.0f / static_cast<float>(settings.spp);
            fb[pixel] = sum * inv;
        }
    }
    return fb;
}

} // namespace pathtracer::render
