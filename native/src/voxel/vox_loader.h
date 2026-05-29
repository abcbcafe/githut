#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "voxel/material_palette.h"
#include "voxel/voxel_chunk.h"

namespace pathtracer::voxel {

struct VoxVoxel {
    uint8_t x, y, z;
    uint8_t color_index; // 1-based index into the .vox palette
};

// Parsed contents of a single-model MagicaVoxel (.vox) file. Palette colors are
// 0xRRGGBBAA; entry [i-1] corresponds to a voxel's 1-based color_index i.
struct VoxScene {
    int size_x = 0, size_y = 0, size_z = 0;
    std::vector<VoxVoxel> voxels;
    std::array<uint32_t, 256> palette{};
    bool has_palette = false;
};

// Parse a .vox byte buffer. Returns nullopt on a bad magic / truncated buffer.
// Handles MAIN/SIZE/XYZI/RGBA; unknown chunks are skipped. Multi-model PACK files
// keep the first model.
std::optional<VoxScene> parse_vox(const std::vector<uint8_t> &bytes);

// Read and parse a .vox file from disk.
std::optional<VoxScene> load_vox_file(const std::string &path);

// Populate a chunk + palette from a parsed scene. Each distinct used color index
// becomes one PbrMaterial (albedo from the .vox palette, or a neutral gray when the
// file has no RGBA chunk). Voxels outside [0,kSize) are clamped out.
void populate_chunk(const VoxScene &scene, VoxelChunk &chunk, MaterialPalette &palette);

} // namespace pathtracer::voxel
