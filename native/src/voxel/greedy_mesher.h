#pragma once

#include "voxel/mesh_data.h"
#include "voxel/voxel_chunk.h"

namespace pathtracer::voxel {

// Greedy meshing: collapse coplanar, adjacent, same-material exposed voxel faces
// into the largest possible quads, then triangulate. A face is emitted only where
// exactly one side is solid (the other is air), so shared faces between two solid
// voxels are hidden regardless of material. Merging never crosses a material or
// orientation boundary.
//
// GPU-free and deterministic, so it is unit-tested on CPU (see native/tests/).
MeshData greedy_mesh(const VoxelChunk &chunk);

} // namespace pathtracer::voxel
