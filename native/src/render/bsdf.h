#pragma once

// Microfacet BSDF building blocks: GGX (Trowbridge-Reitz) normal distribution,
// Smith masking-shadowing, Fresnel-Schlick, and importance sampling of the GGX
// reflection lobe. All functions operate in a local frame whose normal is +z.
//
// These mirror the metallic-roughness model in voxel::PbrMaterial / Godot's
// BaseMaterial3D and back the GPU closest-hit shader. GPU-free and unit-tested
// (NDF normalization, Smith bounds, Fresnel endpoints, single-scatter energy).

#include <cmath>

#include "core/math.h"

namespace pathtracer::render::bsdf {

inline constexpr float kPi = 3.14159265358979323846f;

// GGX/Trowbridge-Reitz normal distribution. cos_h = dot(n, h) >= 0.
inline float ggx_D(float cos_h, float alpha) {
    if (cos_h <= 0.0f) {
        return 0.0f;
    }
    const float a2 = alpha * alpha;
    const float d = cos_h * cos_h * (a2 - 1.0f) + 1.0f;
    return a2 / (kPi * d * d);
}

// Smith masking-shadowing term for one direction (GGX, height-correlated separable).
inline float smith_g1(float cos_v, float alpha) {
    if (cos_v <= 0.0f) {
        return 0.0f;
    }
    const float a2 = alpha * alpha;
    return 2.0f * cos_v / (cos_v + std::sqrt(a2 + (1.0f - a2) * cos_v * cos_v));
}

inline float smith_g(float cos_i, float cos_o, float alpha) {
    return smith_g1(cos_i, alpha) * smith_g1(cos_o, alpha);
}

// Fresnel-Schlick reflectance at the given cosine, with per-channel F0.
inline core::Vec3 fresnel_schlick(float cos_theta, const core::Vec3 &f0) {
    const float m = std::pow(std::fmax(0.0f, 1.0f - cos_theta), 5.0f);
    return {f0.x + (1.0f - f0.x) * m, f0.y + (1.0f - f0.y) * m, f0.z + (1.0f - f0.z) * m};
}

inline core::Vec3 reflect(const core::Vec3 &v, const core::Vec3 &n) {
    return n * (2.0f * core::dot(v, n)) - v;
}

// Microfacet reflection BRDF value for wo, wi in the local frame (z up, both in the
// upper hemisphere). Returns 0 if either direction is below the surface.
inline core::Vec3 microfacet_eval(const core::Vec3 &wo, const core::Vec3 &wi, float alpha,
                                  const core::Vec3 &f0) {
    const float cos_o = wo.z;
    const float cos_i = wi.z;
    if (cos_o <= 0.0f || cos_i <= 0.0f) {
        return {0.0f, 0.0f, 0.0f};
    }
    core::Vec3 h = core::normalize(wo + wi);
    const float cos_h = h.z;
    const float d = ggx_D(cos_h, alpha);
    const float gg = smith_g(cos_i, cos_o, alpha);
    const core::Vec3 f = fresnel_schlick(core::dot(wi, h), f0);
    const float k = d * gg / (4.0f * cos_o * cos_i);
    return f * k;
}

// pdf (w.r.t. solid angle) of the GGX reflection sampler below, for wo, wi local.
inline float microfacet_pdf(const core::Vec3 &wo, const core::Vec3 &wi, float alpha) {
    const core::Vec3 h = core::normalize(wo + wi);
    if (h.z <= 0.0f) {
        return 0.0f;
    }
    const float pdf_h = ggx_D(h.z, alpha) * h.z;
    const float wo_dot_h = core::dot(wo, h);
    if (wo_dot_h <= 0.0f) {
        return 0.0f;
    }
    return pdf_h / (4.0f * wo_dot_h);
}

// Importance-sample the GGX reflection lobe. Returns the sampled incident direction
// wi in the local frame (may be below the surface for grazing wo; caller checks).
inline core::Vec3 microfacet_sample(const core::Vec3 &wo, float alpha, float xi1, float xi2) {
    const float cos_h = std::sqrt((1.0f - xi1) / (1.0f + (alpha * alpha - 1.0f) * xi1));
    const float sin_h = std::sqrt(std::fmax(0.0f, 1.0f - cos_h * cos_h));
    const float phi = 2.0f * kPi * xi2;
    const core::Vec3 h{sin_h * std::cos(phi), sin_h * std::sin(phi), cos_h};
    return reflect(wo, h);
}

} // namespace pathtracer::render::bsdf
