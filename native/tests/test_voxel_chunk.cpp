#include "doctest.h"

#include "voxel/material_palette.h"
#include "voxel/voxel_chunk.h"

using namespace pathtracer::voxel;

TEST_CASE("a new chunk is empty and reads air") {
    VoxelChunk chunk;
    CHECK(chunk.empty());
    CHECK(chunk.at(0, 0, 0) == kAir);
    CHECK_FALSE(chunk.is_solid(5, 5, 5));
}

TEST_CASE("set/at round-trips and marks the chunk non-empty") {
    VoxelChunk chunk;
    chunk.set(1, 2, 3, 7);
    CHECK(chunk.at(1, 2, 3) == 7);
    CHECK(chunk.is_solid(1, 2, 3));
    CHECK_FALSE(chunk.empty());
    // Neighbors stay air.
    CHECK(chunk.at(0, 2, 3) == kAir);
}

TEST_CASE("out-of-bounds is air and writes are ignored") {
    VoxelChunk chunk;
    CHECK(chunk.at(-1, 0, 0) == kAir);
    CHECK(chunk.at(VoxelChunk::kSize, 0, 0) == kAir);
    chunk.set(-1, -1, -1, 9);   // ignored
    chunk.set(VoxelChunk::kSize, 0, 0, 9); // ignored
    CHECK(chunk.empty());
    CHECK_FALSE(VoxelChunk::in_bounds(-1, 0, 0));
    CHECK(VoxelChunk::in_bounds(0, 0, 0));
    CHECK(VoxelChunk::in_bounds(31, 31, 31));
}

TEST_CASE("fill sets every voxel") {
    VoxelChunk chunk;
    chunk.fill(3);
    CHECK_FALSE(chunk.empty());
    CHECK(chunk.at(0, 0, 0) == 3);
    CHECK(chunk.at(31, 31, 31) == 3);
}

TEST_CASE("material palette assigns sequential ids and round-trips") {
    MaterialPalette palette;
    CHECK(palette.size() == 1); // air placeholder

    PbrMaterial stone;
    stone.albedo = {0.5f, 0.5f, 0.5f};
    const MaterialId stone_id = palette.add(stone);
    CHECK(stone_id == 1);

    PbrMaterial lamp;
    lamp.emission = {4.0f, 3.5f, 3.0f};
    const MaterialId lamp_id = palette.add(lamp);
    CHECK(lamp_id == 2);

    CHECK(palette.contains(lamp_id));
    CHECK_FALSE(palette.contains(99));
    CHECK(palette.get(stone_id).albedo[0] == doctest::Approx(0.5f));
    CHECK(palette.get(lamp_id).is_emissive());
    CHECK_FALSE(palette.get(stone_id).is_emissive());
}
