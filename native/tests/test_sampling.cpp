#include "doctest.h"

#include "core/sampling.h"

using namespace pathtracer::core;

TEST_CASE("PCG32 is reproducible and bounded") {
    Pcg32 a(1234), b(1234);
    for (int i = 0; i < 100; ++i) {
        CHECK(a.next_u32() == b.next_u32());
    }
    Pcg32 c(1234), d(5678);
    CHECK(c.next_u32() != d.next_u32());

    Pcg32 r(42);
    for (int i = 0; i < 1000; ++i) {
        const float f = r.next_float();
        CHECK(f >= 0.0f);
        CHECK(f < 1.0f);
    }
}

TEST_CASE("orthonormal basis is orthonormal for varied normals") {
    const Vec3 normals[] = {{0, 0, 1},  {0, 0, -1}, {1, 0, 0},
                            {0, 1, 0},  normalize(Vec3{1, 2, 3}), normalize(Vec3{-2, 0.1f, 0.5f})};
    for (const Vec3 &n : normals) {
        Vec3 t, b;
        build_onb(n, t, b);
        CHECK(length(t) == doctest::Approx(1.0f));
        CHECK(length(b) == doctest::Approx(1.0f));
        CHECK(dot(t, n) == doctest::Approx(0.0f).epsilon(1e-5));
        CHECK(dot(b, n) == doctest::Approx(0.0f).epsilon(1e-5));
        CHECK(dot(t, b) == doctest::Approx(0.0f).epsilon(1e-5));
    }
}

TEST_CASE("cosine hemisphere samples stay in the upper hemisphere and average E[z]=2/3") {
    Pcg32 rng(7);
    double sum_z = 0.0;
    const int n = 200000;
    for (int i = 0; i < n; ++i) {
        const Vec3 s = cosine_sample_hemisphere(rng.next_float(), rng.next_float());
        CHECK(s.z >= -1e-6f);
        CHECK(length(s) == doctest::Approx(1.0f).epsilon(1e-4));
        sum_z += s.z;
    }
    // E[cos theta] for a cosine-weighted hemisphere is 2/3.
    CHECK(sum_z / n == doctest::Approx(2.0 / 3.0).epsilon(0.01));
}

TEST_CASE("to_world maps the local +z axis onto the normal") {
    const Vec3 n = normalize(Vec3{0.3f, 0.7f, -0.2f});
    const Vec3 mapped = to_world(Vec3{0, 0, 1}, n);
    CHECK(mapped.x == doctest::Approx(n.x).epsilon(1e-5));
    CHECK(mapped.y == doctest::Approx(n.y).epsilon(1e-5));
    CHECK(mapped.z == doctest::Approx(n.z).epsilon(1e-5));
}
