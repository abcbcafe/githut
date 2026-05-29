#include "doctest.h"

#include <cmath>

#include "core/math.h"
#include "render/bsdf.h"

using namespace pathtracer;
using pathtracer::core::Vec3;
namespace bsdf = pathtracer::render::bsdf;

TEST_CASE("Fresnel dielectric: normal incidence, grazing, and TIR") {
    // Air -> glass (ior 1.5): R0 = ((1.5-1)/(1.5+1))^2 = 0.04.
    CHECK(bsdf::fresnel_dielectric(1.0f, 1.0f, 1.5f) == doctest::Approx(0.04f));
    // Grazing incidence reflects fully.
    CHECK(bsdf::fresnel_dielectric(0.0f, 1.0f, 1.5f) == doctest::Approx(1.0f));
    // Glass -> air beyond the critical angle (~41.8 deg) is total internal reflection.
    CHECK(bsdf::fresnel_dielectric(std::cos(1.05f), 1.5f, 1.0f) == doctest::Approx(1.0f));
    // Below the critical angle, partial reflection in (0,1).
    const float r = bsdf::fresnel_dielectric(0.99f, 1.5f, 1.0f);
    CHECK(r > 0.0f);
    CHECK(r < 1.0f);
}

TEST_CASE("refraction obeys Snell's law and returns a unit vector") {
    const Vec3 n{0.0f, 0.0f, 1.0f}; // outward normal on the incident side
    const float theta_i = 0.5236f;  // 30 degrees
    const float sin_i = std::sin(theta_i);
    const float cos_i = std::cos(theta_i);
    const Vec3 d{sin_i, 0.0f, -cos_i}; // travels into the surface (dot(d,n) < 0)
    const float eta = 1.0f / 1.5f;     // air -> glass

    Vec3 wt;
    REQUIRE(bsdf::refract(d, n, eta, wt));
    CHECK(core::length(wt) == doctest::Approx(1.0f));
    // Tangential component scales by eta => sin(theta_t) = eta * sin(theta_i).
    CHECK(wt.x == doctest::Approx(eta * sin_i));
    CHECK(wt.z < 0.0f); // continues to the far side
    // Verify Snell directly: n_i sin_i == n_t sin_t.
    const float sin_t = std::sqrt(wt.x * wt.x + wt.y * wt.y);
    CHECK(1.0f * sin_i == doctest::Approx(1.5f * sin_t));
}

TEST_CASE("refraction reports total internal reflection") {
    const Vec3 n{0.0f, 0.0f, 1.0f};
    const float theta_i = 1.05f; // ~60 deg, beyond critical for glass->air
    const Vec3 d{std::sin(theta_i), 0.0f, -std::cos(theta_i)};
    Vec3 wt;
    CHECK_FALSE(bsdf::refract(d, n, 1.5f /*glass->air*/, wt));
}
