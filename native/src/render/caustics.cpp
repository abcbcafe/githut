#include "render/caustics.h"

#include <algorithm>
#include <cmath>
#include <thread>

#include "core/sampling.h"
#include "render/bsdf.h"

namespace pathtracer::render {

using core::Pcg32;
using core::Ray;
using core::Vec3;

namespace {
constexpr float kPi = 3.14159265358979323846f;

Vec3 mul(const Vec3 &a, const Vec3 &b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
float luminance(const Vec3 &c) { return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; }

// Photon (power) transport through a smooth/rough dielectric. Identical to the camera
// dielectric scatter EXCEPT it omits the (eta_i/eta_t)^2 radiance factor, because
// photons carry flux/power (which is conserved across refraction), not radiance.
Vec3 photon_glass(const Vec3 &d, const Vec3 &n_geo, const voxel::PbrMaterial &mat, Pcg32 &rng,
                  Vec3 &flux, bool &inside, Vec3 &sigma) {
    const bool entering = core::dot(d, n_geo) < 0.0f;
    Vec3 ns = entering ? n_geo : -n_geo;
    float eta_i = 1.0f;
    float eta_t = mat.ior;
    if (!entering) {
        std::swap(eta_i, eta_t);
    }
    const float alpha = mat.roughness * mat.roughness;
    Vec3 m = ns;
    if (alpha >= 1e-6f) {
        m = core::normalize(core::to_world(
                bsdf::ggx_sample_normal_local(alpha, rng.next_float(), rng.next_float()), ns));
    }
    const float cos_in = -core::dot(d, m);
    if (cos_in <= 0.0f) {
        flux = Vec3{0, 0, 0};
        return ns;
    }
    const float fr = bsdf::fresnel_dielectric(cos_in, eta_i, eta_t);

    Vec3 wi;
    bool refracted = false;
    Vec3 wt;
    if (rng.next_float() < fr || !bsdf::refract(d, m, eta_i / eta_t, wt)) {
        wi = bsdf::reflect_ray(d, m);
        if (core::dot(wi, ns) <= 0.0f) {
            flux = Vec3{0, 0, 0};
            return ns;
        }
    } else {
        wi = wt;
        refracted = true;
        if (core::dot(wi, ns) >= 0.0f) {
            flux = Vec3{0, 0, 0};
            return ns;
        }
    }
    if (alpha >= 1e-6f) {
        const float g =
                bsdf::smith_g(std::fabs(core::dot(d, ns)), std::fabs(core::dot(wi, ns)), alpha);
        const float w = std::fabs(core::dot(d, m)) * g /
                        (std::fabs(core::dot(d, ns)) * std::fabs(core::dot(m, ns)));
        flux = flux * w;
    }
    if (refracted) {
        inside = !inside;
        sigma = entering ? Vec3{mat.attenuation[0], mat.attenuation[1], mat.attenuation[2]}
                         : Vec3{0, 0, 0};
    }
    return wi;
}

void trace_photons(const TriangleScene &scene, const voxel::MaterialPalette &palette,
                   CausticMap &map, int num_photons, int total_photons, uint64_t seed) {
    const float area = scene.total_emissive_area();
    if (area <= 0.0f) {
        return;
    }
    Pcg32 rng(seed, seed * 2u + 1u);
    constexpr int kMaxBounces = 12;

    for (int i = 0; i < num_photons; ++i) {
        const LightSample ls =
                scene.sample_light(rng.next_float(), rng.next_float(), rng.next_float());
        // Per-photon power: uniform area sampling => power = pi * Le * A_total / N.
        Vec3 flux = mul(ls.emission, Vec3{1, 1, 1}) *
                    (kPi * area / static_cast<float>(total_photons));
        const Vec3 dir =
                core::normalize(core::to_world(
                        core::cosine_sample_hemisphere(rng.next_float(), rng.next_float()),
                        ls.normal));
        Ray ray{ls.point + ls.normal * 1e-3f, dir};

        int specular = 0;
        bool inside = false;
        Vec3 sigma{0, 0, 0};
        for (int b = 0; b < kMaxBounces; ++b) {
            const Hit hit = scene.closest_hit(ray);
            if (!hit.hit) {
                break;
            }
            if (inside) {
                flux = Vec3{flux.x * std::exp(-sigma.x * hit.t), flux.y * std::exp(-sigma.y * hit.t),
                            flux.z * std::exp(-sigma.z * hit.t)};
            }
            const voxel::PbrMaterial &mat =
                    palette.contains(hit.material) ? palette.get(hit.material) : voxel::PbrMaterial{};
            const Vec3 p = ray.origin + ray.dir * hit.t;

            if (mat.is_glass()) {
                const Vec3 nd = photon_glass(ray.dir, hit.normal, mat, rng, flux, inside, sigma);
                ++specular;
                if (luminance(flux) <= 0.0f) {
                    break;
                }
                ray = Ray{p + nd * 1e-3f, nd};
                continue;
            }
            if (mat.is_emissive()) {
                break;
            }
            // Diffuse surface: deposit a caustic only for light that passed through glass,
            // and only on up-facing surfaces (the floor). Direct light (no specular) is
            // handled by the camera path tracer's NEE, so it is excluded here.
            if (specular >= 1 && hit.normal.y > 0.5f) {
                map.splat(p, flux);
            }
            break; // caustic = first diffuse deposit
        }
    }
}
} // namespace

CausticMap::CausticMap(float x0, float z0, float size_x, float size_z, int resolution,
                       float floor_y)
    : x0_(x0), z0_(z0), sx_(size_x), sz_(size_z), floor_y_(floor_y), res_(resolution) {
    cell_area_ = (sx_ / res_) * (sz_ / res_);
    cells_.assign(static_cast<std::size_t>(res_) * res_, Vec3{0, 0, 0});
}

int CausticMap::cell_index(float x, float z) const {
    const int ix = static_cast<int>((x - x0_) / sx_ * res_);
    const int iz = static_cast<int>((z - z0_) / sz_ * res_);
    if (ix < 0 || iz < 0 || ix >= res_ || iz >= res_) {
        return -1;
    }
    return iz * res_ + ix;
}

void CausticMap::splat(const Vec3 &world_pos, const Vec3 &flux) {
    const int idx = cell_index(world_pos.x, world_pos.z);
    if (idx >= 0) {
        cells_[idx] = cells_[idx] + flux;
    }
}

void CausticMap::merge(const CausticMap &other) {
    for (std::size_t i = 0; i < cells_.size() && i < other.cells_.size(); ++i) {
        cells_[i] = cells_[i] + other.cells_[i];
    }
}

Vec3 CausticMap::irradiance(const Vec3 &world_pos) const {
    const int idx = cell_index(world_pos.x, world_pos.z);
    if (idx < 0) {
        return {0, 0, 0};
    }
    return cells_[idx] * (1.0f / cell_area_);
}

Vec3 CausticMap::total_flux() const {
    Vec3 sum{0, 0, 0};
    for (const Vec3 &c : cells_) {
        sum = sum + c;
    }
    return sum;
}

float CausticMap::peak_irradiance() const {
    float peak = 0.0f;
    for (const Vec3 &c : cells_) {
        peak = std::max(peak, luminance(c) / cell_area_);
    }
    return peak;
}

double CausticMap::mean_nonzero_irradiance() const {
    double sum = 0.0;
    int n = 0;
    for (const Vec3 &c : cells_) {
        const float l = luminance(c);
        if (l > 0.0f) {
            sum += l / cell_area_;
            ++n;
        }
    }
    return n > 0 ? sum / n : 0.0;
}

void trace_caustics(const TriangleScene &scene, const voxel::MaterialPalette &palette,
                    CausticMap &map, int num_photons, uint64_t seed) {
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    const unsigned n_threads = std::min<unsigned>(hw, std::max(1, num_photons));
    if (n_threads <= 1) {
        trace_photons(scene, palette, map, num_photons, num_photons, seed);
        return;
    }

    std::vector<CausticMap> locals(n_threads, map); // identical empty layout
    std::vector<std::thread> pool;
    pool.reserve(n_threads);
    const int per = (num_photons + static_cast<int>(n_threads) - 1) / static_cast<int>(n_threads);
    int assigned = 0;
    for (unsigned t = 0; t < n_threads; ++t) {
        const int count = std::min(per, num_photons - assigned);
        assigned += count;
        if (count <= 0) {
            break;
        }
        pool.emplace_back([&, t, count]() {
            trace_photons(scene, palette, locals[t], count, num_photons, seed + t + 1);
        });
    }
    for (std::thread &th : pool) {
        th.join();
    }
    for (const CausticMap &local : locals) {
        map.merge(local);
    }
}

} // namespace pathtracer::render
