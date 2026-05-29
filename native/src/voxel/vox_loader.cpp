#include "voxel/vox_loader.h"

#include <cstring>
#include <fstream>
#include <unordered_map>

namespace pathtracer::voxel {
namespace {

// Little-endian readers with bounds checking via a moving cursor.
class Reader {
public:
    Reader(const uint8_t *data, std::size_t size) : data_(data), size_(size) {}

    bool ok() const { return ok_; }
    std::size_t pos() const { return pos_; }
    bool at_end() const { return pos_ >= size_; }

    bool read_tag(char out[4]) {
        if (pos_ + 4 > size_) {
            return fail();
        }
        std::memcpy(out, data_ + pos_, 4);
        pos_ += 4;
        return true;
    }

    int32_t read_i32() {
        if (pos_ + 4 > size_) {
            fail();
            return 0;
        }
        int32_t v = static_cast<int32_t>(data_[pos_]) | (static_cast<int32_t>(data_[pos_ + 1]) << 8)
                | (static_cast<int32_t>(data_[pos_ + 2]) << 16)
                | (static_cast<int32_t>(data_[pos_ + 3]) << 24);
        pos_ += 4;
        return v;
    }

    uint8_t read_u8() {
        if (pos_ >= size_) {
            fail();
            return 0;
        }
        return data_[pos_++];
    }

    void skip(std::size_t n) {
        if (pos_ + n > size_) {
            fail();
            return;
        }
        pos_ += n;
    }

private:
    bool fail() {
        ok_ = false;
        return false;
    }

    const uint8_t *data_;
    std::size_t size_;
    std::size_t pos_ = 0;
    bool ok_ = true;
};

bool tag_is(const char tag[4], const char *name) { return std::memcmp(tag, name, 4) == 0; }

} // namespace

std::optional<VoxScene> parse_vox(const std::vector<uint8_t> &bytes) {
    Reader r(bytes.data(), bytes.size());

    char magic[4];
    if (!r.read_tag(magic) || !tag_is(magic, "VOX ")) {
        return std::nullopt;
    }
    r.read_i32(); // version

    char main_tag[4];
    if (!r.read_tag(main_tag) || !tag_is(main_tag, "MAIN")) {
        return std::nullopt;
    }
    r.read_i32(); // MAIN content size (0)
    r.read_i32(); // MAIN children size

    VoxScene scene;
    bool have_size = false;
    bool have_voxels = false;

    while (r.ok() && !r.at_end()) {
        char tag[4];
        if (!r.read_tag(tag)) {
            break;
        }
        const int32_t content_size = r.read_i32();
        r.read_i32(); // children size
        if (!r.ok() || content_size < 0) {
            break;
        }
        const std::size_t content_start = r.pos();

        if (tag_is(tag, "SIZE")) {
            // Note .vox is Z-up; we keep its axes as-is here.
            scene.size_x = r.read_i32();
            scene.size_y = r.read_i32();
            scene.size_z = r.read_i32();
            have_size = true;
        } else if (tag_is(tag, "XYZI") && !have_voxels) {
            const int32_t count = r.read_i32();
            if (count > 0 && r.ok()) {
                scene.voxels.reserve(static_cast<std::size_t>(count));
                for (int32_t i = 0; i < count && r.ok(); ++i) {
                    VoxVoxel vx;
                    vx.x = r.read_u8();
                    vx.y = r.read_u8();
                    vx.z = r.read_u8();
                    vx.color_index = r.read_u8();
                    scene.voxels.push_back(vx);
                }
            }
            have_voxels = true;
        } else if (tag_is(tag, "RGBA")) {
            for (int i = 0; i < 256 && r.ok(); ++i) {
                const uint8_t rr = r.read_u8();
                const uint8_t gg = r.read_u8();
                const uint8_t bb = r.read_u8();
                const uint8_t aa = r.read_u8();
                scene.palette[i] = (static_cast<uint32_t>(rr) << 24)
                        | (static_cast<uint32_t>(gg) << 16) | (static_cast<uint32_t>(bb) << 8)
                        | static_cast<uint32_t>(aa);
            }
            scene.has_palette = true;
        }

        // Always advance to the declared end of this chunk's content.
        r.skip(content_start + static_cast<std::size_t>(content_size) - r.pos());
    }

    if (!have_size || !have_voxels) {
        return std::nullopt;
    }
    return scene;
}

std::optional<VoxScene> load_vox_file(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    return parse_vox(bytes);
}

void populate_chunk(const VoxScene &scene, VoxelChunk &chunk, MaterialPalette &palette) {
    // .vox color index (1..255) -> our MaterialId, created on first use.
    std::unordered_map<uint8_t, MaterialId> color_to_material;

    for (const VoxVoxel &vx : scene.voxels) {
        if (!VoxelChunk::in_bounds(vx.x, vx.y, vx.z)) {
            continue;
        }
        auto it = color_to_material.find(vx.color_index);
        MaterialId material;
        if (it == color_to_material.end()) {
            PbrMaterial mat;
            if (scene.has_palette && vx.color_index >= 1) {
                const uint32_t rgba = scene.palette[vx.color_index - 1];
                mat.albedo = {((rgba >> 24) & 0xFF) / 255.0f, ((rgba >> 16) & 0xFF) / 255.0f,
                              ((rgba >> 8) & 0xFF) / 255.0f};
            }
            material = palette.add(mat);
            color_to_material.emplace(vx.color_index, material);
        } else {
            material = it->second;
        }
        chunk.set(vx.x, vx.y, vx.z, material);
    }
}

} // namespace pathtracer::voxel
