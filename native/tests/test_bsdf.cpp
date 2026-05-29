#include "doctest.h"

#include <cmath>

#include "core/sampling.h"
#include "render/bsdf.h"

using namespace pathtracer;
using pathtracer::core::Pcg32;
using pathtracer::core::Vec3;
namespace bsdf = pathtracer::render::bsdf;

namespace {
// Uniform hemisphere direction (z up), pdf = 1/2pi.
Vec3 uniform_hemisphere(float u1, float u2) {
    const float z = u1;
    const float r = std::sqrt(std::fmax(0.0f, 1.0f - z * z));
    const float phi = 2.0f * bsdf::kPi * u2;
    return {r * std::cos(phi), r * std::sin(phi), z};
}
} // namespace

TEST_CASE("GGX NDF integrates to 1 (integral of D*cos over the hemisphere)") {
    Pcg32 rng(5);
    for (float alpha : {0.5f, 0.8f, 1.0f}) {
        double sum = 0.0;
        const int n = 400000;
        for (int i = 0; i < n; ++i) {
            const Vec3 h = uniform_hemisphere(rng.next_float(), rng.next_float());
            sum += bsdf::ggx_D(h.z, alpha) * h.z;
        }
        const double integral = (sum / n) * 2.0 * M_PI; // divide by uniform pdf 1/2pi
        CHECK(integral == doctest::Approx(1.0).epsilon(0.03));
    }
}

TEST_CASE("Smith G1 is bounded in (0, 1]") {
    for (float alpha : {0.1f, 0.5f, 1.0f}) {
        for (float c : {0.05f, 0.3f, 0.7f, 1.0f}) {
            const float g1 = bsdf::smith_g1(c, alpha);
            CHECK(g1 > 0.0f);
            CHECK(g1 <= 1.0f + 1e-5f);
        }
    }
    CHECK(bsdf::smith_g1(-0.1f, 0.5f) == 0.0f); // below surface
}

TEST_CASE("Fresnel-Schlick endpoints") {
    const Vec3 f0{0.04f, 0.04f, 0.04f};
    const Vec3 at_normal = bsdf::fresnel_schlick(1.0f, f0);
    CHECK(at_normal.x == doctest::Approx(0.04f));
    const Vec3 at_grazing = bsdf::fresnel_schlick(0.0f, f0);
    CHECK(at_grazing.x == doctest::Approx(1.0f));
    CHECK(at_grazing.z == doctest::Approx(1.0f));
}

TEST_CASE("microfacet reflection conserves energy (directional albedo <= 1)") {
    Pcg32 rng(23);
    const Vec3 f0{1.0f, 1.0f, 1.0f}; // perfect conductor isolates the G/D energy term
    for (float alpha : {0.2f, 0.5f, 0.8f}) {
        for (float cos_o : {0.95f, 0.5f}) {
            const float sin_o = std::sqrt(1.0f - cos_o * cos_o);
            const Vec3 wo{sin_o, 0.0f, cos_o};

            double rho = 0.0;
            const int n = 200000;
            for (int i = 0; i < n; ++i) {
                const Vec3 wi = bsdf::microfacet_sample(wo, alpha, rng.next_float(), rng.next_float());
                if (wi.z <= 0.0f) {
                    continue; // reflected below surface: no contribution
                }
                const float pdf = bsdf::microfacet_pdf(wo, wi, alpha);
                if (pdf <= 0.0f) {
                    continue;
                }
                const Vec3 f = bsdf::microfacet_eval(wo, wi, alpha, f0);
                rho += static_cast<double>(f.x) * wi.z / pdf; // f * cos / pdf
            }
            rho /= n;
            CHECK(rho > 0.0);
            CHECK(rho <= 1.0 + 1e-2); // single-scatter: <= 1, less for rough
        }
    }
}
