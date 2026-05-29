#include "doctest.h"

#include "core/math.h"
#include "render/camera.h"

using namespace pathtracer::core;
using pathtracer::render::PinholeCamera;

TEST_CASE("vector basics") {
    const Vec3 a{1, 2, 3};
    const Vec3 b{4, 5, 6};
    CHECK(dot(a, b) == doctest::Approx(32.0f));
    const Vec3 c = cross({1, 0, 0}, {0, 1, 0});
    CHECK(c.x == doctest::Approx(0.0f));
    CHECK(c.y == doctest::Approx(0.0f));
    CHECK(c.z == doctest::Approx(1.0f));
    CHECK(length(normalize(Vec3{0, 3, 4})) == doctest::Approx(1.0f));
}

TEST_CASE("ray/triangle intersection (Moller-Trumbore)") {
    const Triangle tri{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};

    SUBCASE("hit through the interior") {
        const Ray ray{{0.25f, 0.25f, -1.0f}, {0, 0, 1}};
        const auto t = intersect(ray, tri);
        REQUIRE(t.has_value());
        CHECK(*t == doctest::Approx(1.0f));
    }
    SUBCASE("miss outside the triangle") {
        const Ray ray{{2.0f, 2.0f, -1.0f}, {0, 0, 1}};
        CHECK_FALSE(intersect(ray, tri).has_value());
    }
    SUBCASE("triangle behind the ray is not hit") {
        const Ray ray{{0.25f, 0.25f, 1.0f}, {0, 0, 1}};
        CHECK_FALSE(intersect(ray, tri).has_value());
    }
    SUBCASE("parallel ray misses") {
        const Ray ray{{0.25f, 0.25f, -1.0f}, {1, 0, 0}};
        CHECK_FALSE(intersect(ray, tri).has_value());
    }
}

TEST_CASE("ray/AABB slab test") {
    const Aabb box{{-1, -1, -1}, {1, 1, 1}};
    CHECK(intersect(Ray{{0, 0, -5}, {0, 0, 1}}, box));
    CHECK_FALSE(intersect(Ray{{5, 5, -5}, {0, 0, 1}}, box));
}

TEST_CASE("pinhole camera primary rays") {
    PinholeCamera cam(/*position*/ {0, 0, 0}, /*forward*/ {0, 0, -1}, /*up*/ {0, 1, 0},
                      /*fov_y*/ 1.5707963f /*90deg*/, /*aspect*/ 2.0f);

    SUBCASE("center ray looks straight forward") {
        const Ray r = cam.generate_ray(0.5f, 0.5f);
        CHECK(r.dir.x == doctest::Approx(0.0f));
        CHECK(r.dir.y == doctest::Approx(0.0f));
        CHECK(r.dir.z == doctest::Approx(-1.0f));
    }
    SUBCASE("right edge tilts toward +x; aspect widens horizontally") {
        const Ray r = cam.generate_ray(1.0f, 0.5f);
        CHECK(r.dir.x > 0.0f);
        // With 90deg vertical fov and aspect 2, the horizontal half-extent is 2,
        // so the right-edge ray has a larger x component than z is deep.
        CHECK(r.dir.x > -r.dir.z);
    }
    SUBCASE("all rays are normalized") {
        const Ray r = cam.generate_ray(0.0f, 1.0f);
        CHECK(length(r.dir) == doctest::Approx(1.0f));
    }
}
