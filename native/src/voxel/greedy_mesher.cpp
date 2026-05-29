#include "voxel/greedy_mesher.h"

#include <array>
#include <cstdlib>

namespace pathtracer::voxel {
namespace {

constexpr int kSize = VoxelChunk::kSize;

// Mask cell: 0 = no face; otherwise the magnitude is the material id and the sign
// encodes orientation (positive => face points along +axis, owned by the lower
// voxel; negative => face points along -axis). Encoding both in one value means
// quads only merge when material *and* orientation match.
using MaskCell = int32_t;

void emit_quad(MeshData &mesh, int d, int u, int v, const std::array<int, 3> &origin,
               int w, int h, MaskCell cell) {
    const bool positive = cell > 0;
    const auto material = static_cast<MaterialId>(std::abs(cell));

    std::array<float, 3> base{static_cast<float>(origin[0]), static_cast<float>(origin[1]),
                              static_cast<float>(origin[2])};
    std::array<float, 3> du{0, 0, 0};
    std::array<float, 3> dv{0, 0, 0};
    du[u] = static_cast<float>(w);
    dv[v] = static_cast<float>(h);

    std::array<float, 3> normal{0, 0, 0};
    normal[d] = positive ? 1.0f : -1.0f;

    const auto base_index = static_cast<uint32_t>(mesh.vertices.size());

    auto push = [&](const std::array<float, 3> &p, float uu, float vv) {
        mesh.vertices.push_back(Vertex{p[0], p[1], p[2], normal[0], normal[1], normal[2], uu, vv,
                                       material});
    };

    const std::array<float, 3> p0 = base;
    const std::array<float, 3> p1{base[0] + du[0], base[1] + du[1], base[2] + du[2]};
    const std::array<float, 3> p2{base[0] + du[0] + dv[0], base[1] + du[1] + dv[1],
                                  base[2] + du[2] + dv[2]};
    const std::array<float, 3> p3{base[0] + dv[0], base[1] + dv[1], base[2] + dv[2]};

    const auto fw = static_cast<float>(w);
    const auto fh = static_cast<float>(h);
    push(p0, 0.0f, 0.0f);
    push(p1, fw, 0.0f);
    push(p2, fw, fh);
    push(p3, 0.0f, fh);

    // Wind triangles so the front face matches the normal direction.
    if (positive) {
        for (uint32_t i : {0u, 1u, 2u, 0u, 2u, 3u}) {
            mesh.indices.push_back(base_index + i);
        }
    } else {
        for (uint32_t i : {0u, 2u, 1u, 0u, 3u, 2u}) {
            mesh.indices.push_back(base_index + i);
        }
    }
    ++mesh.quad_count;
}

} // namespace

MeshData greedy_mesh(const VoxelChunk &chunk) {
    MeshData mesh;

    std::array<MaskCell, kSize * kSize> mask{};

    // Sweep the three axes; for each, build a 2D mask per slice and merge.
    for (int d = 0; d < 3; ++d) {
        const int u = (d + 1) % 3;
        const int v = (d + 2) % 3;

        std::array<int, 3> x{0, 0, 0};
        std::array<int, 3> q{0, 0, 0};
        q[d] = 1;

        for (x[d] = -1; x[d] < kSize;) {
            // Build the mask of faces between slice x[d] and x[d]+1.
            int n = 0;
            for (x[v] = 0; x[v] < kSize; ++x[v]) {
                for (x[u] = 0; x[u] < kSize; ++x[u], ++n) {
                    const MaterialId a = (x[d] >= 0) ? chunk.at(x[0], x[1], x[2]) : kAir;
                    const MaterialId b = (x[d] < kSize - 1)
                            ? chunk.at(x[0] + q[0], x[1] + q[1], x[2] + q[2])
                            : kAir;
                    const bool a_solid = a != kAir;
                    const bool b_solid = b != kAir;

                    if (a_solid == b_solid) {
                        mask[n] = 0; // both air or both solid => face hidden
                    } else if (a_solid) {
                        mask[n] = static_cast<MaskCell>(a); // face points +d
                    } else {
                        mask[n] = -static_cast<MaskCell>(b); // face points -d
                    }
                }
            }

            ++x[d];

            // Greedily merge the mask into quads.
            n = 0;
            for (int j = 0; j < kSize; ++j) {
                for (int i = 0; i < kSize;) {
                    const MaskCell cell = mask[n];
                    if (cell == 0) {
                        ++i;
                        ++n;
                        continue;
                    }

                    // Extend width along u.
                    int w = 1;
                    while (i + w < kSize && mask[n + w] == cell) {
                        ++w;
                    }

                    // Extend height along v while every cell in the row matches.
                    int h = 1;
                    bool stop = false;
                    while (j + h < kSize && !stop) {
                        for (int k = 0; k < w; ++k) {
                            if (mask[n + k + h * kSize] != cell) {
                                stop = true;
                                break;
                            }
                        }
                        if (!stop) {
                            ++h;
                        }
                    }

                    std::array<int, 3> origin{0, 0, 0};
                    origin[d] = x[d];
                    origin[u] = i;
                    origin[v] = j;
                    emit_quad(mesh, d, u, v, origin, w, h, cell);

                    // Clear the consumed region.
                    for (int l = 0; l < h; ++l) {
                        for (int k = 0; k < w; ++k) {
                            mask[n + k + l * kSize] = 0;
                        }
                    }

                    i += w;
                    n += w;
                }
            }
        }
    }

    return mesh;
}

} // namespace pathtracer::voxel
