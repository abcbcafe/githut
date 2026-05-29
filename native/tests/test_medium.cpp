#include "doctest.h"

#include <cmath>

#include "core/sampling.h"
#include "render/medium.h"

using namespace pathtracer;
using pathtracer::core::Pcg32;
using pathtracer::core::Vec3;
using pathtracer::render::HomogeneousMedium;

TEST_CASE("Beer-Lambert transmittance is exact and per-channel") {
    HomogeneousMedium m;
    m.sigma_a = {0.2f, 0.4f, 0.6f};
    m.sigma_s = {0.0f, 0.0f, 0.0f};
    CHECK(m.sigma_t().y == doctest::Approx(0.4f));

    const Vec3 tr = transmittance(m, 3.0f);
    CHECK(tr.x == doctest::Approx(std::exp(-0.6f)));
    CHECK(tr.y == doctest::Approx(std::exp(-1.2f)));
    CHECK(tr.z == doctest::Approx(std::exp(-1.8f)));

    HomogeneousMedium vac;
    CHECK(vac.is_vacuum());
}

TEST_CASE("free-flight sampling: mean free path 1/sigma_t and correct survival") {
    const float sigma_t = 0.5f;
    Pcg32 rng(3);
    double sum = 0.0;
    const int n = 400000;
    int survive_past_d = 0;
    const float d = 2.0f;
    for (int i = 0; i < n; ++i) {
        const float s = render::sample_distance(sigma_t, rng.next_float());
        CHECK(s >= 0.0f);
        sum += s;
        if (s > d) {
            ++survive_past_d;
        }
    }
    CHECK(sum / n == doctest::Approx(1.0 / sigma_t).epsilon(0.01)); // mean free path
    // P(no collision before d) == transmittance exp(-sigma_t d).
    CHECK(static_cast<double>(survive_past_d) / n ==
          doctest::Approx(std::exp(-sigma_t * d)).epsilon(0.01));
}

TEST_CASE("Henyey-Greenstein phase integrates to 1 over the sphere") {
    Pcg32 rng(11);
    for (float g : {-0.5f, 0.0f, 0.3f, 0.7f}) {
        double sum = 0.0;
        const int n = 300000;
        for (int i = 0; i < n; ++i) {
            // Uniform sphere direction; cos in [-1,1].
            const float u = rng.next_float();
            const float cos_theta = 1.0f - 2.0f * u;
            sum += render::hg_phase(cos_theta, g);
        }
        // Integral over sphere = mean(phase) * 4*pi (uniform sphere pdf = 1/4pi).
        const double integral = (sum / n) * 4.0 * M_PI;
        CHECK(integral == doctest::Approx(1.0).epsilon(0.02));
    }
}

TEST_CASE("Henyey-Greenstein sampling has mean cosine g") {
    Pcg32 rng(17);
    const Vec3 forward{0.0f, 0.0f, 1.0f};
    for (float g : {-0.4f, 0.0f, 0.6f}) {
        double sum_cos = 0.0;
        const int n = 300000;
        for (int i = 0; i < n; ++i) {
            const Vec3 d = render::hg_sample(forward, g, rng.next_float(), rng.next_float());
            CHECK(core::length(d) == doctest::Approx(1.0f).epsilon(1e-3));
            sum_cos += core::dot(d, forward);
        }
        CHECK(sum_cos / n == doctest::Approx(g).epsilon(0.01));
    }
}
