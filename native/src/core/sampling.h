#pragma once

// Random number generation + Monte Carlo sampling helpers shared by the CPU path
// integrator (and mirrored by the GPU shaders). GPU-free and deterministic so the
// estimators can be unit-tested (white-furnace energy conservation, reproducibility).

#include <cstdint>

#include "core/math.h"

namespace pathtracer::core {

// PCG32 — small, fast, well-distributed, and reproducible from a seed.
class Pcg32 {
public:
    explicit Pcg32(uint64_t seed = 0x853c49e6748fea9bULL, uint64_t seq = 0xda3e39cb94b95bdbULL) {
        state_ = 0;
        inc_ = (seq << 1u) | 1u;
        next_u32();
        state_ += seed;
        next_u32();
    }

    uint32_t next_u32() {
        const uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + inc_;
        const uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
        const uint32_t rot = static_cast<uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
    }

    // Uniform float in [0, 1).
    float next_float() {
        return static_cast<float>(next_u32()) * (1.0f / 4294967296.0f);
    }

private:
    uint64_t state_;
    uint64_t inc_;
};

// Branchless orthonormal basis around a unit normal (Duff et al. 2017).
inline void build_onb(const Vec3 &n, Vec3 &t, Vec3 &b) {
    const float sign = n.z >= 0.0f ? 1.0f : -1.0f;
    const float a = -1.0f / (sign + n.z);
    const float d = n.x * n.y * a;
    t = Vec3{1.0f + sign * n.x * n.x * a, sign * d, -sign * n.x};
    b = Vec3{d, sign + n.y * n.y * a, -n.y};
}

// Cosine-weighted hemisphere sample in the local frame (z = up). pdf = cos / pi.
inline Vec3 cosine_sample_hemisphere(float u1, float u2) {
    const float r = std::sqrt(u1);
    const float phi = 6.2831853071795864769f * u2;
    const float x = r * std::cos(phi);
    const float y = r * std::sin(phi);
    const float z = std::sqrt(std::fmax(0.0f, 1.0f - u1));
    return Vec3{x, y, z};
}

// Transform a local-frame direction into world space around normal n.
inline Vec3 to_world(const Vec3 &local, const Vec3 &n) {
    Vec3 t, b;
    build_onb(n, t, b);
    return Vec3{t.x * local.x + b.x * local.y + n.x * local.z,
                t.y * local.x + b.y * local.y + n.y * local.z,
                t.z * local.x + b.z * local.y + n.z * local.z};
}

} // namespace pathtracer::core
