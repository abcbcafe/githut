#include "doctest.h"

#include <cstdint>
#include <vector>

#include "voxel/greedy_mesher.h"
#include "voxel/vox_loader.h"

using namespace pathtracer::voxel;

namespace {

// Helper to assemble a minimal .vox byte buffer.
struct VoxBuilder {
    std::vector<uint8_t> bytes;

    void tag(const char *t) {
        bytes.insert(bytes.end(), t, t + 4);
    }
    void i32(int32_t v) {
        bytes.push_back(static_cast<uint8_t>(v & 0xFF));
        bytes.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    }
    void u8(uint8_t v) { bytes.push_back(v); }
};

// Two voxels (different colors) in a 2x2x2 model, plus an RGBA palette.
std::vector<uint8_t> make_two_voxel_vox() {
    VoxBuilder b;
    b.tag("VOX ");
    b.i32(150);
    b.tag("MAIN");
    b.i32(0); // content
    b.i32(0); // children size (parser reads to EOF, so value is unused)

    b.tag("SIZE");
    b.i32(12);
    b.i32(0);
    b.i32(2);
    b.i32(2);
    b.i32(2);

    b.tag("XYZI");
    b.i32(4 + 4 * 2);
    b.i32(0);
    b.i32(2); // voxel count
    b.u8(0); b.u8(0); b.u8(0); b.u8(1); // voxel @ origin, color 1
    b.u8(1); b.u8(0); b.u8(0); b.u8(2); // voxel @ +x, color 2

    b.tag("RGBA");
    b.i32(1024);
    b.i32(0);
    for (int i = 0; i < 256; ++i) {
        if (i == 0) { b.u8(255); b.u8(0); b.u8(0); b.u8(255); }      // index 1 -> red
        else if (i == 1) { b.u8(0); b.u8(255); b.u8(0); b.u8(255); } // index 2 -> green
        else { b.u8(0); b.u8(0); b.u8(0); b.u8(255); }
    }
    return b.bytes;
}

} // namespace

TEST_CASE("parse a well-formed .vox buffer") {
    const auto scene = parse_vox(make_two_voxel_vox());
    REQUIRE(scene.has_value());
    CHECK(scene->size_x == 2);
    CHECK(scene->size_y == 2);
    CHECK(scene->size_z == 2);
    REQUIRE(scene->voxels.size() == 2);
    CHECK(scene->voxels[0].color_index == 1);
    CHECK(scene->voxels[1].color_index == 2);
    REQUIRE(scene->has_palette);
    CHECK(scene->palette[0] == 0xFF0000FFu); // red, RRGGBBAA
    CHECK(scene->palette[1] == 0x00FF00FFu); // green
}

TEST_CASE("reject buffers with a bad magic or truncation") {
    CHECK_FALSE(parse_vox({'N', 'O', 'P', 'E'}).has_value());
    auto truncated = make_two_voxel_vox();
    truncated.resize(6); // cut off mid-header
    CHECK_FALSE(parse_vox(truncated).has_value());
}

TEST_CASE("populate a chunk + palette and feed the greedy mesher") {
    const auto scene = parse_vox(make_two_voxel_vox());
    REQUIRE(scene.has_value());

    VoxelChunk chunk;
    MaterialPalette palette;
    populate_chunk(*scene, chunk, palette);

    CHECK(palette.size() == 3); // air + 2 distinct colors
    CHECK(chunk.is_solid(0, 0, 0));
    CHECK(chunk.is_solid(1, 0, 0));
    const MaterialId m0 = chunk.at(0, 0, 0);
    const MaterialId m1 = chunk.at(1, 0, 0);
    CHECK(m0 != m1);
    CHECK(palette.get(m0).albedo[0] == doctest::Approx(1.0f)); // red albedo
    CHECK(palette.get(m1).albedo[1] == doctest::Approx(1.0f)); // green albedo

    // End-to-end: two adjacent different-material voxels mesh to 10 quads
    // (shared face hidden, sides not merged across the material boundary).
    const MeshData mesh = greedy_mesh(chunk);
    CHECK(mesh.quad_count == 10);
}
