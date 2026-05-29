#pragma once

// Minimal, dependency-free linear-algebra + intersection primitives shared by the
// renderer (CPU reference path tracer, ray setup) and the geometric acoustic tracer.
// Kept free of Godot/Vulkan so it is unit-tested on CPU.

#include <cmath>
#include <optional>

namespace pathtracer::core {

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3() = default;
    Vec3(float px, float py, float pz) : x(px), y(py), z(pz) {}

    Vec3 operator+(const Vec3 &o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3 &o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
};

inline float dot(const Vec3 &a, const Vec3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 cross(const Vec3 &a, const Vec3 &b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float length(const Vec3 &v) { return std::sqrt(dot(v, v)); }

inline Vec3 normalize(const Vec3 &v) {
    const float len = length(v);
    return len > 0.0f ? v * (1.0f / len) : v;
}

struct Ray {
    Vec3 origin;
    Vec3 dir; // expected normalized
};

struct Triangle {
    Vec3 a, b, c;
};

// Axis-aligned bounding box.
struct Aabb {
    Vec3 min;
    Vec3 max;

    void expand(const Vec3 &p) {
        min = {std::fmin(min.x, p.x), std::fmin(min.y, p.y), std::fmin(min.z, p.z)};
        max = {std::fmax(max.x, p.x), std::fmax(max.y, p.y), std::fmax(max.z, p.z)};
    }
};

// Möller–Trumbore ray/triangle intersection. Returns the hit distance t > epsilon
// along the ray, or nullopt. Single-sided is not assumed (both faces hit).
inline std::optional<float> intersect(const Ray &ray, const Triangle &tri, float epsilon = 1e-6f) {
    const Vec3 e1 = tri.b - tri.a;
    const Vec3 e2 = tri.c - tri.a;
    const Vec3 p = cross(ray.dir, e2);
    const float det = dot(e1, p);
    if (std::fabs(det) < epsilon) {
        return std::nullopt; // ray parallel to triangle
    }
    const float inv_det = 1.0f / det;
    const Vec3 t_vec = ray.origin - tri.a;
    const float u = dot(t_vec, p) * inv_det;
    if (u < 0.0f || u > 1.0f) {
        return std::nullopt;
    }
    const Vec3 q = cross(t_vec, e1);
    const float v = dot(ray.dir, q) * inv_det;
    if (v < 0.0f || u + v > 1.0f) {
        return std::nullopt;
    }
    const float t = dot(e2, q) * inv_det;
    if (t <= epsilon) {
        return std::nullopt; // intersection behind the origin
    }
    return t;
}

// Slab test for ray/AABB. Returns true if the ray hits the box within [0, t_max].
inline bool intersect(const Ray &ray, const Aabb &box, float t_max = 1e30f) {
    float t0 = 0.0f;
    float t1 = t_max;
    const float o[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float d[3] = {ray.dir.x, ray.dir.y, ray.dir.z};
    const float bmin[3] = {box.min.x, box.min.y, box.min.z};
    const float bmax[3] = {box.max.x, box.max.y, box.max.z};
    for (int i = 0; i < 3; ++i) {
        const float inv = 1.0f / d[i];
        float tn = (bmin[i] - o[i]) * inv;
        float tf = (bmax[i] - o[i]) * inv;
        if (tn > tf) {
            std::swap(tn, tf);
        }
        t0 = tn > t0 ? tn : t0;
        t1 = tf < t1 ? tf : t1;
        if (t0 > t1) {
            return false;
        }
    }
    return true;
}

} // namespace pathtracer::core
