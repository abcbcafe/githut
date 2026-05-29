#pragma once

// Participating-medium primitives for the "air" volumetrics: a homogeneous medium
// described by per-channel absorption/scattering coefficients plus a Henyey-
// Greenstein phase function. These are the exact building blocks the GPU volumetric
// integrator will use; they are GPU-free and unit-tested (Beer-Lambert transmittance,
// mean free path, phase normalization and mean cosine).

#include <cmath>

#include "core/math.h"
#include "core/sampling.h"

namespace pathtracer::render {

struct HomogeneousMedium {
    core::Vec3 sigma_a{0.0f, 0.0f, 0.0f}; // absorption coefficient (per channel)
    core::Vec3 sigma_s{0.0f, 0.0f, 0.0f}; // scattering coefficient (per channel)
    float g = 0.0f;                       // HG anisotropy in (-1, 1); 0 = isotropic

    core::Vec3 sigma_t() const { return sigma_a + sigma_s; }
    bool is_vacuum() const {
        const core::Vec3 t = sigma_t();
        return t.x <= 0.0f && t.y <= 0.0f && t.z <= 0.0f;
    }
};

// Beer-Lambert transmittance over a path of length `dist` (per channel).
inline core::Vec3 transmittance(const HomogeneousMedium &m, float dist) {
    const core::Vec3 t = m.sigma_t();
    return {std::exp(-t.x * dist), std::exp(-t.y * dist), std::exp(-t.z * dist)};
}

// Free-flight distance sampling for a scalar extinction. pdf(t) = sigma_t e^{-sigma_t t};
// the mean sampled distance is the mean free path 1/sigma_t.
inline float sample_distance(float sigma_t, float xi) {
    return -std::log(1.0f - xi) / sigma_t;
}
inline float distance_pdf(float sigma_t, float t) {
    return sigma_t * std::exp(-sigma_t * t);
}

// Henyey-Greenstein phase function, normalized over the sphere (integrates to 1).
// `cos_theta` is the cosine between the forward (incoming) direction and the
// scattered direction.
inline float hg_phase(float cos_theta, float g) {
    const float denom = 1.0f + g * g - 2.0f * g * cos_theta;
    return (1.0f / (4.0f * 3.14159265358979323846f)) * (1.0f - g * g) /
           (denom * std::sqrt(std::fmax(denom, 1e-8f)));
}

// Sample a scattered direction around the forward direction `wo` (the ray's travel
// direction). The pdf w.r.t. solid angle equals hg_phase(cos_theta, g).
inline core::Vec3 hg_sample(const core::Vec3 &wo, float g, float xi1, float xi2) {
    float cos_theta;
    if (std::fabs(g) < 1e-3f) {
        cos_theta = 1.0f - 2.0f * xi1; // isotropic
    } else {
        const float sqr = (1.0f - g * g) / (1.0f - g + 2.0f * g * xi1);
        cos_theta = (1.0f + g * g - sqr * sqr) / (2.0f * g);
    }
    const float sin_theta = std::sqrt(std::fmax(0.0f, 1.0f - cos_theta * cos_theta));
    const float phi = 6.2831853071795864769f * xi2;
    const core::Vec3 local{sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta};
    return core::normalize(core::to_world(local, core::normalize(wo)));
}

} // namespace pathtracer::render
