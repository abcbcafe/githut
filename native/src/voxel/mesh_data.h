#pragma once

#include <cstdint>
#include <vector>

#include "voxel/voxel_chunk.h"

namespace pathtracer::voxel {

// Plain, GPU-agnostic triangle mesh produced by the greedy mesher. The render
// layer uploads this into vertex/index buffers and builds a BLAS from it; the
// audio layer can reuse the same geometry. Material id is carried per-vertex so
// the renderer can group draws / shader records by material.
struct Vertex {
    float px, py, pz; // position (voxel units)
    float nx, ny, nz; // face normal
    float u, v;       // planar UV in voxel units
    MaterialId material;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t quad_count = 0; // merged faces emitted (each = 2 triangles)

    bool empty() const { return indices.empty(); }
    uint32_t triangle_count() const { return static_cast<uint32_t>(indices.size() / 3); }
};

} // namespace pathtracer::voxel
