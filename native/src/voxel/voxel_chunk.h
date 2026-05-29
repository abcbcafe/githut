#pragma once

#include <array>
#include <cstdint>

namespace pathtracer::voxel {

// Material id 0 is reserved for "air" / empty. Non-zero ids index into the
// global voxel material palette (see material_palette.h).
using MaterialId = uint16_t;
inline constexpr MaterialId kAir = 0;

// A fixed-size cubic chunk of coarse voxels, stored densely. Coarse blocks +
// "performance is not an early priority" make a dense array the right call;
// palette compression can come later.
//
// This type is deliberately free of any Godot or Vulkan dependency so it can be
// unit-tested on CPU (see native/tests/). The renderer consumes the greedy-meshed
// output (mesh_data.h) rather than this structure directly.
class VoxelChunk {
public:
    static constexpr int kSize = 32;
    static constexpr int kVolume = kSize * kSize * kSize;

    VoxelChunk() { voxels_.fill(kAir); }

    static constexpr bool in_bounds(int x, int y, int z) {
        return x >= 0 && y >= 0 && z >= 0 && x < kSize && y < kSize && z < kSize;
    }

    // Out-of-bounds reads return air, so a standalone chunk meshes its outer
    // shell. (Cross-chunk neighbor sampling is a later addition.)
    MaterialId at(int x, int y, int z) const {
        if (!in_bounds(x, y, z)) {
            return kAir;
        }
        return voxels_[index(x, y, z)];
    }

    void set(int x, int y, int z, MaterialId material) {
        if (in_bounds(x, y, z)) {
            voxels_[index(x, y, z)] = material;
        }
    }

    bool is_solid(int x, int y, int z) const { return at(x, y, z) != kAir; }

    void fill(MaterialId material) { voxels_.fill(material); }

    bool empty() const {
        for (MaterialId v : voxels_) {
            if (v != kAir) {
                return false;
            }
        }
        return true;
    }

private:
    static constexpr int index(int x, int y, int z) {
        return (z * kSize + y) * kSize + x;
    }

    std::array<MaterialId, kVolume> voxels_{};
};

} // namespace pathtracer::voxel
