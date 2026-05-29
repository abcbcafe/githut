#include "doctest.h"

#include "core/math.h"
#include "core/sampling.h"
#include "render/cpu_reference.h"
#include "voxel/greedy_mesher.h"
#include "voxel/voxel_chunk.h"

using namespace pathtracer;
using pathtracer::core::Pcg32;
using pathtracer::core::Ray;
using pathtracer::core::Vec3;
using pathtracer::render::TriangleScene;

namespace {

// Eight isolated voxels => 96 triangles, enough to exercise BVH branching.
TriangleScene scattered_scene() {
    voxel::VoxelChunk chunk;
    const int p[8][3] = {{0, 0, 0}, {5, 0, 0}, {0, 5, 0}, {0, 0, 5},
                         {5, 5, 0}, {5, 0, 5}, {0, 5, 5}, {5, 5, 5}};
    for (auto &v : p) {
        chunk.set(v[0], v[1], v[2], 1);
    }
    TriangleScene scene;
    scene.add_mesh(voxel::greedy_mesh(chunk));
    return scene;
}

} // namespace

TEST_CASE("BVH closest_hit matches brute force over random rays") {
    TriangleScene scene = scattered_scene();
    REQUIRE(scene.triangle_count() == 96); // 8 voxels * 12 triangles, none merged
    scene.build_bvh();
    REQUIRE(scene.has_bvh());

    Pcg32 rng(2024);
    int hits = 0;
    for (int i = 0; i < 5000; ++i) {
        const Vec3 origin{rng.next_float() * 10.0f - 2.0f, rng.next_float() * 10.0f - 2.0f,
                          rng.next_float() * 10.0f - 2.0f};
        Vec3 d{rng.next_float() * 2.0f - 1.0f, rng.next_float() * 2.0f - 1.0f,
               rng.next_float() * 2.0f - 1.0f};
        if (core::length(d) < 1e-3f) {
            continue;
        }
        const Ray ray{origin, core::normalize(d)};

        const auto reference = scene.brute_force_closest_hit(ray);
        const auto accel = scene.closest_hit(ray);

        CHECK(accel.hit == reference.hit);
        if (reference.hit) {
            ++hits;
            CHECK(accel.t == doctest::Approx(reference.t).epsilon(1e-4));
            CHECK(accel.material == reference.material);
        }
    }
    CHECK(hits > 0); // sanity: rays actually struck geometry
}

TEST_CASE("BVH any_hit matches brute force (shadow queries)") {
    TriangleScene scene = scattered_scene();
    scene.build_bvh();

    Pcg32 rng(99);
    for (int i = 0; i < 5000; ++i) {
        const Vec3 origin{rng.next_float() * 10.0f - 2.0f, rng.next_float() * 10.0f - 2.0f,
                          rng.next_float() * 10.0f - 2.0f};
        Vec3 d{rng.next_float() * 2.0f - 1.0f, rng.next_float() * 2.0f - 1.0f,
               rng.next_float() * 2.0f - 1.0f};
        if (core::length(d) < 1e-3f) {
            continue;
        }
        const Ray ray{origin, core::normalize(d)};
        const float max_t = 3.0f + rng.next_float() * 10.0f;
        CHECK(scene.any_hit(ray, max_t) == scene.brute_force_any_hit(ray, max_t));
    }
}
