#include "doctest.h"

#include <cmath>
#include <map>

#include "voxel/greedy_mesher.h"
#include "voxel/voxel_chunk.h"

using namespace pathtracer::voxel;

namespace {

// Structural invariants every greedy mesh must satisfy.
void check_invariants(const MeshData &mesh) {
    CHECK(mesh.vertices.size() == 4u * mesh.quad_count);
    CHECK(mesh.indices.size() == 6u * mesh.quad_count);
    CHECK(mesh.triangle_count() == 2u * mesh.quad_count);
    for (const Vertex &vtx : mesh.vertices) {
        // Air never produces a face.
        CHECK(vtx.material != kAir);
        // Normals are axis-aligned unit vectors.
        const float len = std::sqrt(vtx.nx * vtx.nx + vtx.ny * vtx.ny + vtx.nz * vtx.nz);
        CHECK(len == doctest::Approx(1.0f));
    }
    // Every index references a real vertex.
    for (uint32_t idx : mesh.indices) {
        CHECK(idx < mesh.vertices.size());
    }
}

// Quads are emitted as runs of 4 consecutive vertices with a uniform material.
std::map<MaterialId, int> quads_per_material(const MeshData &mesh) {
    std::map<MaterialId, int> counts;
    for (std::size_t i = 0; i < mesh.vertices.size(); i += 4) {
        counts[mesh.vertices[i].material]++;
    }
    return counts;
}

} // namespace

TEST_CASE("empty chunk produces no geometry") {
    VoxelChunk chunk;
    const MeshData mesh = greedy_mesh(chunk);
    CHECK(mesh.empty());
    CHECK(mesh.quad_count == 0);
    check_invariants(mesh);
}

TEST_CASE("a single voxel has six faces") {
    VoxelChunk chunk;
    chunk.set(4, 4, 4, 1);
    const MeshData mesh = greedy_mesh(chunk);
    CHECK(mesh.quad_count == 6);
    CHECK(mesh.vertices.size() == 24);
    CHECK(mesh.triangle_count() == 12);
    check_invariants(mesh);
}

TEST_CASE("a 2x2x2 solid block merges each side into one quad") {
    VoxelChunk chunk;
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                chunk.set(x, y, z, 1);
            }
        }
    }
    const MeshData mesh = greedy_mesh(chunk);
    // Without merging this would be 6 faces * 4 = 24 quads; greedy gives 6.
    CHECK(mesh.quad_count == 6);
    check_invariants(mesh);
}

TEST_CASE("a fully solid chunk meshes to its six outer faces") {
    VoxelChunk chunk;
    chunk.fill(1);
    const MeshData mesh = greedy_mesh(chunk);
    CHECK(mesh.quad_count == 6);
    check_invariants(mesh);
}

TEST_CASE("adjacent same-material voxels hide the shared face and merge sides") {
    VoxelChunk chunk;
    chunk.set(0, 0, 0, 1);
    chunk.set(1, 0, 0, 1); // 2x1x1 run along x
    const MeshData mesh = greedy_mesh(chunk);
    // Two end caps (1x1) + four sides each merged across the length (2x1) = 6.
    CHECK(mesh.quad_count == 6);
    check_invariants(mesh);
}

TEST_CASE("a material boundary prevents merging but keeps the internal face hidden") {
    VoxelChunk chunk;
    chunk.set(0, 0, 0, 1);
    chunk.set(1, 0, 0, 2); // different material, still solid
    const MeshData mesh = greedy_mesh(chunk);
    // Internal shared face hidden (both solid). Each of the 4 side directions now
    // has two unmergeable quads (different materials), plus the two end caps: 10.
    CHECK(mesh.quad_count == 10);
    check_invariants(mesh);

    const auto counts = quads_per_material(mesh);
    CHECK(counts.at(1) == 5);
    CHECK(counts.at(2) == 5);
}
