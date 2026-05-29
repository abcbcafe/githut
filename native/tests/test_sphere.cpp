#include "doctest.h"

#include "core/math.h"
#include "render/cpu_reference.h"
#include "voxel/greedy_mesher.h"
#include "voxel/voxel_chunk.h"

using namespace pathtracer;
using pathtracer::core::Ray;
using pathtracer::core::Vec3;
using pathtracer::render::TriangleScene;

TEST_CASE("ray hits an analytic sphere with the correct distance and normal") {
    TriangleScene scene;
    scene.add_sphere({0, 0, 0}, 1.0f, 7);

    const Ray ray{{0, 0, 5}, {0, 0, -1}};
    const auto hit = scene.closest_hit(ray);
    REQUIRE(hit.hit);
    CHECK(hit.t == doctest::Approx(4.0f)); // 5 - radius
    CHECK(hit.material == 7);
    CHECK(hit.normal.z == doctest::Approx(1.0f)); // outward normal at the near pole
}

TEST_CASE("ray misses a sphere it passes beside") {
    TriangleScene scene;
    scene.add_sphere({0, 0, 0}, 1.0f, 7);
    CHECK_FALSE(scene.closest_hit(Ray{{3, 0, 5}, {0, 0, -1}}).hit);
}

TEST_CASE("a ray from inside the sphere hits the far wall (exit point)") {
    TriangleScene scene;
    scene.add_sphere({0, 0, 0}, 2.0f, 1);
    const auto hit = scene.closest_hit(Ray{{0, 0, 0}, {0, 0, -1}});
    REQUIRE(hit.hit);
    CHECK(hit.t == doctest::Approx(2.0f));
    // The outward geometric normal at the exit points along -z (away from center).
    CHECK(hit.normal.z == doctest::Approx(-1.0f));
}

TEST_CASE("spheres and triangles share the scene; the nearest wins") {
    voxel::VoxelChunk chunk;
    chunk.set(0, 0, 0, 1); // a voxel spanning [0,1]^3, far side faces toward +z at z=1
    TriangleScene scene;
    scene.add_mesh(voxel::greedy_mesh(chunk));
    scene.build_bvh();
    scene.add_sphere({0.5f, 0.5f, 4.0f}, 0.6f, 9); // closer to the camera at z>1

    const Ray ray{{0.5f, 0.5f, 10.0f}, {0, 0, -1}};
    const auto hit = scene.closest_hit(ray);
    REQUIRE(hit.hit);
    CHECK(hit.material == 9); // the sphere occludes the voxel
    CHECK(hit.t == doctest::Approx(10.0f - 4.6f));
    CHECK(scene.any_hit(ray, 100.0f));
}
