#pragma once

// A binary BVH over a triangle soup — the CPU analogue of the GPU BLAS. Median-split
// build, stack-based traversal. Used to accelerate the reference path tracer's hit
// queries; verified against brute force in the unit tests.

#include <vector>

#include "core/math.h"

namespace pathtracer::render {

class Bvh {
public:
    struct Hit {
        bool hit = false;
        float t = 0.0f;
        int index = -1; // index into the triangle list passed to build()
    };

    // Copies the triangles so the BVH is self-contained.
    void build(const std::vector<core::Triangle> &triangles);

    Hit closest(const core::Ray &ray) const;
    bool any(const core::Ray &ray, float max_t) const;

    bool empty() const { return triangles_.empty(); }
    std::size_t node_count() const { return nodes_.size(); }

private:
    struct Node {
        core::Aabb box;
        int left_or_first = 0; // internal: left child node index; leaf: first ordered index
        int right_child = 0;   // internal: right child node index (subtrees aren't contiguous)
        int count = 0;         // 0 => internal node; >0 => leaf triangle count
    };

    int build_recursive(int start, int end, const std::vector<core::Aabb> &boxes,
                         const std::vector<core::Vec3> &centroids);

    std::vector<core::Triangle> triangles_;
    std::vector<int> ordered_; // permutation of triangle indices grouped into leaves
    std::vector<Node> nodes_;
};

} // namespace pathtracer::render
